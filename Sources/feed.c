#include "plataforma_mensagens.h"



int main(int argc, char *argv[])
{
    char nome_user[TAM_MAX_NOME_UTILIZADOR];
    FeedData feed_data = { .num_topicos_subscritos = 0, .bem_vindo_recebido = 0, .inscricao_confirmada = 0 };

    // Verifica se o nome do utilizador foi passado como argumento
    if (argc > 1)
    {
        snprintf(nome_user, sizeof(nome_user), "%s", argv[1]);
        nome_user[strcspn(nome_user, "\n")] = '\0';
    }
    else
    {
        printf("Forma de uso: %s <nome_utilizador>\n", argv[0]);
        return 1;
    }

    // Cria o FIFO do cliente
    snprintf(feed_data.pipe_cliente, sizeof(feed_data.pipe_cliente), "CLI_FIFO_%d", getpid());
    if (access(feed_data.pipe_cliente, F_OK) != 0)
    {
        if (mkfifo(feed_data.pipe_cliente, 0666) == -1)
        {
            perror("Erro ao criar o FIFO do cliente");
            return 1;
        }
    }

    iniciar_feed(nome_user, &feed_data);

    return 0;
}
// Função para iniciar o feed de mensagens
// Envia uma mensagem de inscrição para o manager
// Cria um FIFO exclusivo para o cliente
// Cria uma thread para receber mensagens do manager
// Inicia um ciclo para tratar os comandos do utilizador
void iniciar_feed(const char *nome_utilizador, FeedData *feed_data)
{
    if (strcmp(nome_utilizador, "") == 0)
    {
        printf("Erro: Nome do utilizador não especificado.\n");
        exit(0);
    }

    MensagemManager mensagem;
    mensagem.tipo = MSG_INSCRICAO;
    snprintf(mensagem.nome_utilizador, sizeof(mensagem.nome_utilizador), "%s", nome_utilizador);
    snprintf(mensagem.nome_pipe, sizeof(mensagem.nome_pipe), "%s", feed_data->pipe_cliente);
    mensagem.nome_topico[0] = '\0';

    enviar_mensagem(&mensagem);

    int pipe_fd = open(feed_data->pipe_cliente, O_RDWR);
    if (pipe_fd == -1)
    {
        perror("Erro ao abrir o FIFO do cliente");
        unlink(feed_data->pipe_cliente);
        exit(1);
    }

    pthread_t thread_receptor;
    if (pthread_create(&thread_receptor, NULL, thread_receber_mensagens, (void *)feed_data) != 0)
    {
        perror("Erro ao criar a thread de recepção de mensagens");
        close(pipe_fd);
        exit(1);
    }
    feed_data->thread_comandos = pthread_self();
    // Espera até receber a mensagem de boas-vindas
    while (!feed_data->bem_vindo_recebido)
    {
        usleep(100000);
    }

    tratar_comandos_utilizador(nome_utilizador, feed_data);

    pthread_cancel(thread_receptor);
    pthread_join(thread_receptor, NULL);
    close(pipe_fd);
    unlink(feed_data->pipe_cliente);
}
// Função para tratar os comandos do utilizador
void tratar_comandos_utilizador(const char *nome_utilizador, FeedData *feed_data)
{
    struct sigaction sa;
    sa.sa_handler = signal_handler; 
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    char linha[512];
    MensagemManager mensagem;
    strncpy(mensagem.nome_utilizador, nome_utilizador, TAM_MAX_NOME_UTILIZADOR - 1);
    mensagem.nome_utilizador[TAM_MAX_NOME_UTILIZADOR - 1] = '\0';

    strncpy(mensagem.nome_pipe, feed_data->pipe_cliente, sizeof(mensagem.nome_pipe) - 1);
    mensagem.nome_pipe[sizeof(mensagem.nome_pipe) - 1] = '\0';

    while (1)
    {
        listar_comandos();
        printf("Comando: ");
        if (fgets(linha, sizeof(linha), stdin) == NULL)
            continue;
        linha[strcspn(linha, "\n")] = '\0';

        char *comando = strtok(linha, " ");
        // Se o comando for "sub", subscreve um tópico localmente e envia uma mensagem ao manager
        if (strcmp(comando, "sub") == 0)
        {
            char *nome_topico = strtok(NULL, " ");
            if (!nome_topico)
            {
                printf("Erro: Nome do tópico não especificado.\n");
                continue;
            }
            if (strlen(nome_topico) > TAM_MAX_TOPICO - 1)
            {
                printf("AVISO: O nome do tópico excede o limite de %d caracteres.O topico será criado com o numero maximo de caracteres permitido.\n", TAM_MAX_TOPICO - 1);
                nome_topico[TAM_MAX_TOPICO - 1] = '\0';
            }

            mensagem.tipo = MSG_SUBSCRICAO;
            snprintf(mensagem.nome_utilizador, sizeof(mensagem.nome_utilizador), "%s", nome_utilizador);
            snprintf(mensagem.nome_topico, sizeof(mensagem.nome_topico), "%s", nome_topico);

            enviar_mensagem(&mensagem);
            adicionar_topico(nome_topico, feed_data);
        }
        // Se o comando for "unsub", cancela a subscrição de um tópico localmente e envia uma mensagem ao manager
        else if (strcmp(comando, "unsub") == 0)
        {
            char *nome_topico = strtok(NULL, " ");
            if (!nome_topico)
            {
                printf("Erro: Nome do tópico não especificado.\n");
                continue;
            }

            if (!verificar_inscricao(nome_topico, feed_data))
            {
                printf("Erro: Não está inscrito no tópico '%s'.\n", nome_topico);
                continue;
            }

            mensagem.tipo = MSG_DESUBSCRICAO;
            snprintf(mensagem.nome_topico, sizeof(mensagem.nome_topico), "%s", nome_topico);
            snprintf(mensagem.nome_utilizador, sizeof(mensagem.nome_utilizador), "%s", nome_utilizador);

            enviar_mensagem(&mensagem);
            cancelar_subscricao_topico(nome_topico, feed_data);
        }
        // Se o comando for "msg", envia uma mensagem para um tópico
        else if (strcmp(comando, "msg") == 0)
        {
            mensagem.tipo = MSG_PUBLICACAO;
            char *nome_topico = strtok(NULL, " ");
            char *duracao_str = strtok(NULL, " ");
            char *conteudo = strtok(NULL, "");

            if (nome_topico == NULL || duracao_str == NULL || conteudo == NULL)
            {
                printf("Erro: Formato inválido. Use: msg <topico> <duracao> <mensagem>\n");
                continue;
            }
            if (strlen(conteudo) > TAM_MAX_MSG - 1)
            {
                printf("AVISO: O conteúdo da mensagem excede o limite de %d caracteres. A mensagem será enviada com o numero máximo de cracteres possível.\n", TAM_MAX_MSG - 1);
                conteudo[TAM_MAX_MSG - 1] = '\0'; 
            }
            snprintf(mensagem.nome_pipe, sizeof(mensagem.nome_pipe), "%s", feed_data->pipe_cliente);
            snprintf(mensagem.nome_topico, sizeof(mensagem.nome_topico), "%s", nome_topico);
            mensagem.duracao = atoi(duracao_str);
            snprintf(mensagem.conteudo, sizeof(mensagem.conteudo), "%s", conteudo);
            enviar_mensagem(&mensagem);
        }
        //Se o comando for "topics" envia uma mensagem ao manager a pedir todos os topicos disponiveis
        else if (strcmp(comando, "topics") == 0)
        {
            MensagemManager mensagem;
            mensagem.tipo = MSG_TOPICOS;
            snprintf(mensagem.nome_utilizador, sizeof(mensagem.nome_utilizador), "%s", nome_utilizador);
            snprintf(mensagem.nome_pipe, sizeof(mensagem.nome_pipe), "%s", feed_data->pipe_cliente);
            enviar_mensagem(&mensagem);
            printf("Solicitação de lista de tópicos enviada ao manager.\n");
        }
        // Se o comando for "topics-subscritos", lista os tópicos subscritos
        else if (strcmp(comando, "topics-subscritos") == 0)
        {
            listar_topicos_subscritos(feed_data);
        }
        // Se o comando for "exit", envia uma mensagem ao manager e encerra o programa
        else if (strcmp(comando, "exit") == 0)
        {
            mensagem.tipo = MSG_REMOVER;
            enviar_mensagem(&mensagem);
            printf("A sair...\n");
            unlink(feed_data->pipe_cliente);
            exit(0);
            break;
        }
        else
        {
            printf("Comando inválido.\n");
        }
    }
}

// Função para receber mensagens do manager
void *thread_receber_mensagens(void *arg)
{
    FeedData *feed_data = (FeedData *)arg;
    int pipe_fd = open(feed_data->pipe_cliente, O_RDWR);
    MensagemManager mensagem;

    while (1)
    {
        ssize_t bytes_lidos = read(pipe_fd, &mensagem, sizeof(MensagemManager));
        if (bytes_lidos > 0)
        {
            switch (mensagem.tipo)
            {
            // Mensagem de publicação de uma mensagem
            // Imprime o autor, o tópico e o conteúdo da mensagem
            case MSG_PUBLICACAO:
                printf("\n[Mensagem recebida]\n");
                printf("  Autor: %s\n", mensagem.nome_utilizador);
                printf("  Tópico: %s\n", mensagem.nome_topico);
                printf("  Conteúdo: %s\n", mensagem.conteudo);
                printf("Comando: \n");
                break;
            // Mensagem de bloqueio de um tópico
            // Imprime o tópico bloqueado
            case MSG_BLOQUEIO:
                printf("\n[Mensagem de Bloqueio]\n");
                printf("  Tópico '%s' bloqueado.\n", mensagem.nome_topico, mensagem.nome_utilizador);
                printf("Comando: \n");
                break;
            // Mensagem de desbloqueio de um tópico
            // Imprime o tópico desbloqueado
            case MSG_DESBLOQUEIO:
                printf("\n[Mensagem de Desbloqueio]\n");
                printf("  Tópico '%s' desbloqueado.\n", mensagem.nome_topico, mensagem.nome_utilizador);
                printf("Comando: \n");
                break;
            // Mensagem de remoção de um utilizador
            // Imprime o nome do utilizador removido
            // Se o utilizador removido for o utilizador atual, encerra o programa
            // Se o utilizador removido não for o utilizador atual, imprime uma mensagem informativa
            case MSG_REMOVER:
                if (strcmp(mensagem.nome_pipe, feed_data->pipe_cliente) == 0)
                {
                    printf("\n[Alerta]\n");
                    printf("  O manager notificou a remoção do utilizador '%s'.\n", mensagem.nome_utilizador);
                    printf("  O programa será encerrado.\n");
                    // Envia o sinal de encerramento para a thread de comandos
                    unlink(feed_data->pipe_cliente);
                    pthread_kill(feed_data->thread_comandos, SIGUSR1);  // Aqui, SIGUSR1 em vez de SIGTERM
                    close(pipe_fd);
                    pthread_exit(NULL);
                }
                else
                {
                    printf("INFO: O utilizador '%s' saiu da plataforma.\n", mensagem.nome_utilizador);
                    printf("Comando: \n");
                }
                break;
            // Mensagem de subscrição de um tópico
            // Imprime o resultado da subscrição
            // Se a subscrição for bem-sucedida, adiciona o tópico à lista de tópicos subscritos
            // Se a subscrição não for bem-sucedida, imprime uma mensagem de erro
            case MSG_SUBSCRICAO:
                printf("\n[Resposta do Manager]\n");
                printf("  Subscrição no tópico '%s': %s\n", mensagem.nome_topico, mensagem.conteudo);
                if (strstr(mensagem.conteudo, "bem-sucedida"))
                {
                    feed_data->inscricao_confirmada = 1;
                    adicionar_topico(mensagem.nome_topico, feed_data);
                }
                printf("Comando: \n");
                break;
            // Mensagem de cancelamento de subscrição de um tópico
            // Imprime o resultado do cancelamento da subscrição
            // Se o cancelamento for bem-sucedido, remove o tópico da lista de tópicos subscritos
            // Se o cancelamento não for bem-sucedido, imprime uma mensagem de erro
            case MSG_DESUBSCRICAO:
                printf("\n[Resposta do Manager]\n");
                printf("  Cancelamento da subscrição no tópico '%s': %s\n", mensagem.nome_topico, mensagem.conteudo);
                if (strstr(mensagem.conteudo, "bem-sucedida"))
                {
                    cancelar_subscricao_topico(mensagem.nome_topico, feed_data);
                }
                printf("Comando: \n");
                break;
            // Mensagem de inscrição no feed
            // Imprime o resultado da inscrição
            // Se a inscrição for bem-sucedida, imprime uma mensagem de boas-vindas
            // Se a inscrição não for bem-sucedida, imprime uma mensagem de erro e encerra o programa
            case MSG_INSCRICAO:
                printf("\n[Resposta do Manager]\n");
                printf("  Inscrição: %s\n", mensagem.conteudo);
                if (strlen(mensagem.conteudo) > 33)
                {
                    printf("  A sair...\n");
                    unlink(feed_data->pipe_cliente);
                    pthread_kill(feed_data->thread_comandos, SIGUSR1);  
                    close(pipe_fd);                    
                    pthread_exit(NULL);
                }
                feed_data->bem_vindo_recebido = 1;
                break;
            // Mensagem de tópicos disponíveis
            // Imprime a lista de tópicos disponíveis
            case MSG_TOPICOS:
                printf("\n[Topicos Disponíveis]\n");
                printf("%s\n", mensagem.conteudo);
                printf("Comando: \n");
                break;
            default:
                printf("\n[Mensagem desconhecida]\n");
                printf("  Tipo de mensagem não reconhecido.\n");
                break;
            }
        }
        else
        {
            perror("Erro ao ler mensagem do pipe");
            usleep(100000);
        }
    }
    return NULL;
}
// Função para listar os comandos disponíveis
void listar_comandos()
{
    printf("\n Comandos disponíveis (um por linha):\n");
    printf("  topics                     - Lista os tópicos existentes.\n");
    printf("  topics-subscritos          - Lista os tópicos subscritos.\n");
    printf("  sub <topico>               - Subscreve um tópico.\n");
    printf("  unsub <topico>             - Cancela a subscrição de um tópico.\n");
    printf("  msg <topico> <duracao> <mensagem> - Envia uma mensagem para um tópico.\n");
    printf("  exit                       - Sai do feed e encerra o programa.\n");
    printf("\n");
}
// Função para enviar uma mensagem ao manager
// Abre o pipe do manager e envia a mensagem
// Se o pipe não existir, imprime uma mensagem de erro e encerra o programa
// Se a mensagem for enviada com sucesso, imprime uma mensagem informativa
void enviar_mensagem(const MensagemManager *mensagem)
{
    if (access(PIPE_MANAGER, F_OK) != 0)
    {
        fprintf(stderr, "Erro: Pipe do manager não encontrado.\n");
        exit(1);
    }

    int pipe_manager_fd = open(PIPE_MANAGER, O_RDWR);
    if (pipe_manager_fd == -1)
    {
        perror("Erro ao abrir o pipe do manager");
        exit(1);
    }

    MensagemManager mensagem_enviada = *mensagem;

    write(pipe_manager_fd, &mensagem_enviada, sizeof(MensagemManager));
    close(pipe_manager_fd);
}
// Função para listar os tópicos subscritos
// Percorre a lista de tópicos subscritos e imprime os tópicos subscritos
// Se o utilizador não estiver subscrito em nenhum tópico, imprime uma mensagem informativa
// Se o utilizador estiver subscrito em tópicos, imprime os tópicos subscritos
void listar_topicos_subscritos(FeedData *feed_data)
{
    if (feed_data->num_topicos_subscritos == 0)
    {
        printf("O utilizador não está subscrito em nenhum tópico.\n");
        return;
    }

    printf("\nTópicos subscritos:\n");
    for (int i = 0; i < feed_data->num_topicos_subscritos; i++)
    {
        if (feed_data->topicos_subscritos[i].subscrito)
        {
            printf("- %s\n", feed_data->topicos_subscritos[i].nome_topico);
        }
    }
    printf("\n");
}
// Função para adicionar um tópico à lista de tópicos subscritos
// Se o tópico já estiver subscrito, imprime uma mensagem informativa
// Se o tópico não estiver subscrito, adiciona o tópico à lista de tópicos subscritos
void adicionar_topico(const char *nome_topico, FeedData *feed_data)
{
    if (feed_data->num_topicos_subscritos >= MAX_TOPICOS_SUBSCRITOS)
    {
        printf("Erro: Limite de tópicos subscritos atingido.\n");
        return;
    }

    for (int i = 0; i < feed_data->num_topicos_subscritos; i++)
    {
        if (strcmp(feed_data->topicos_subscritos[i].nome_topico, nome_topico) == 0)
        {
            if (feed_data->topicos_subscritos[i].subscrito == 1)
            {
                printf("Já se encontra subscrito no tópico '%s'.\n", nome_topico);
                return;
            }
            else
            {
                feed_data->topicos_subscritos[i].subscrito = 1;
                printf("Inscrição no tópico '%s' reativada.\n", nome_topico);
                return;
            }
        }
    }

    snprintf(feed_data->topicos_subscritos[feed_data->num_topicos_subscritos].nome_topico, TAM_MAX_TOPICO, "%s", nome_topico);
    feed_data->topicos_subscritos[feed_data->num_topicos_subscritos].subscrito = 1;
    feed_data->num_topicos_subscritos++;
    printf("Tópico '%s' subscrito com sucesso.\n", nome_topico);
}
// Função para verificar se um tópico está subscrito
// Percorre a lista de tópicos subscritos e verifica se o tópico está subscrito
// Se o tópico estiver subscrito, retorna 1
int verificar_inscricao(const char *nome_topico, FeedData *feed_data)
{
    for (int i = 0; i < feed_data->num_topicos_subscritos; i++)
    {
        if (strcmp(feed_data->topicos_subscritos[i].nome_topico, nome_topico) == 0 && feed_data->topicos_subscritos[i].subscrito == 1)
        {
            return 1;
        }
    }
    return 0;
}
// Função para cancelar a subscrição de um tópico
// Percorre a lista de tópicos subscritos e cancela a subscrição do tópico
// Se o tópico não for encontrado, imprime uma mensagem de erro
// Se o tópico for encontrado, cancela a subscrição do tópico
// Se a subscrição for cancelada com sucesso, imprime uma mensagem informativa
void cancelar_subscricao_topico(const char *nome_topico, FeedData *feed_data)
{
    for (int i = 0; i < feed_data->num_topicos_subscritos; i++)
    {
        if (strcmp(feed_data->topicos_subscritos[i].nome_topico, nome_topico) == 0)
        {
            feed_data->topicos_subscritos[i].subscrito = 0;
            printf("Tópico '%s' dessubscrito com sucesso.\n", nome_topico);
            return;
        }
    }
    printf("Erro: Não foi possível encontrar o tópico '%s'.\n", nome_topico);
}

void signal_handler(int signal)
{
    if (signal == SIGUSR1)
    {
        printf("A encerrar o programa...\n");
        exit(0);
    }
}
