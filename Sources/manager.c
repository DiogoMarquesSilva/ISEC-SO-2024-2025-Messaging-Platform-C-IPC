#include "plataforma_mensagens.h"

int main()
{
    ManagerData manager_data;
    manager_data.num_utilizadores = 0;
    manager_data.num_topicos = 0;
    pthread_mutex_init(&manager_data.mutex_utilizadores, NULL);
    pthread_mutex_init(&manager_data.mutex_mensagens, NULL);

    carregar_mensagens_do_ficheiro(&manager_data);

    pthread_t thread_admin, thread_tempo;
    
    // Criação de threads para tratar comandos do administrador e atualizar o tempo das mensagens
    if (pthread_create(&thread_admin, NULL, comandos_admin_thread, &manager_data) != 0)
    {
        perror("Erro ao criar thread de comandos do administrador");
        exit(1);
    }
    if (pthread_create(&thread_tempo, NULL, (void *)atualizar_tempo_mensagens, &manager_data) != 0)
    {
        perror("Erro ao criar thread para atualizar tempo das mensagens");
        exit(1);
    }

    iniciar_manager(&manager_data);

    pthread_join(thread_admin, NULL);
    pthread_join(thread_tempo, NULL);

    pthread_mutex_destroy(&manager_data.mutex_utilizadores);
    pthread_mutex_destroy(&manager_data.mutex_mensagens);

    return 0;
}

void *comandos_admin_thread(void *arg)
{
    ManagerData *manager_data = (ManagerData *)arg;
    while (1)
    {
        tratar_comandos_admin(manager_data);
    }
    return NULL;
}
// Função para colocar o manager à escuta de mensagens
void iniciar_manager(ManagerData *manager_data)
{
    if (access(PIPE_MANAGER, F_OK) != 0)
    {
        if (mkfifo(PIPE_MANAGER, 0666) == -1)
        {
            perror("Erro ao criar o pipe do manager");
            exit(1);
        }
    }

    while (1)
    {
        int pipe_manager_fd = open(PIPE_MANAGER, O_RDWR);
        if (pipe_manager_fd == -1)
        {
            perror("Erro ao abrir o pipe do manager");
            break;
        }

        MensagemManager mensagem;
        read(pipe_manager_fd, &mensagem, sizeof(MensagemManager));
        close(pipe_manager_fd);

        tratar_mensagem(&mensagem, manager_data);
    }

    unlink(PIPE_MANAGER);
}

// Função para tratar os comandos do administrador
void tratar_mensagem(const MensagemManager *mensagem, ManagerData *manager_data)
{
    switch (mensagem->tipo)
    {
    // Mensagem de inscrição de um novo utilizador
    // Cria um novo utilizador e envia uma mensagem de confirmação
    // Se o número máximo de utilizadores for atingido, envia uma mensagem de erro
    case MSG_INSCRICAO:
    {
        UtilizadorManager novo_utilizador;

        snprintf(novo_utilizador.nome_utilizador, sizeof(novo_utilizador.nome_utilizador), "%s", mensagem->nome_utilizador);
        snprintf(novo_utilizador.nome_pipe, sizeof(novo_utilizador.nome_pipe), "%s", mensagem->nome_pipe);
        novo_utilizador.num_topicos = 0;
        printf("\nO utilizador '%s' quer entrar na plataforma\n", novo_utilizador.nome_utilizador);

        // Chamar a função para adicionar o utilizador
        const char *mensagem_resposta = adicionar_utilizador(&novo_utilizador, manager_data);

        // Enviar a resposta ao cliente
        int pipe_fd = open(novo_utilizador.nome_pipe, O_WRONLY);
        if (pipe_fd != -1)
        {
            MensagemManager resposta;
            resposta.tipo = MSG_INSCRICAO;
            snprintf(resposta.conteudo, sizeof(resposta.conteudo), "%s", mensagem_resposta);
            write(pipe_fd, &resposta, sizeof(MensagemManager));
            close(pipe_fd);
        }
        else
        {
            perror("Erro ao abrir o FIFO do cliente para confirmação");
        }

        break;
    }

    // Mensagem de subscrição de um tópico
    // Adiciona o utilizador ao tópico e envia uma mensagem de confirmação
    // Se o utilizador já estiver subscrito, envia uma mensagem de erro
    case MSG_SUBSCRICAO:
    {
        printf("\n INFO: Utilizador '%s' quer subscrever o tópico '%s'.\n",
               mensagem->nome_utilizador, mensagem->nome_topico);

        int indice_topico = obter_ou_criar_topico(mensagem->nome_topico, manager_data);
        if (indice_topico == -1)
        {
            printf("Erro: Limite de tópicos atingido.\n");
            return;
        }

        if (!adicionar_subscritor(mensagem->nome_topico, mensagem->nome_pipe, mensagem->nome_utilizador, manager_data))
        {
            printf("Erro: Utilizador '%s' já está subscrito no tópico '%s'.\n",
                   mensagem->nome_utilizador, mensagem->nome_topico);

            MensagemManager resposta;
            resposta.tipo = MSG_SUBSCRICAO;
            snprintf(resposta.nome_utilizador, sizeof(resposta.nome_utilizador), "%s", mensagem->nome_utilizador);
            snprintf(resposta.nome_topico, sizeof(resposta.nome_topico), "%s", mensagem->nome_topico);
            snprintf(resposta.conteudo, sizeof(resposta.conteudo), "Erro: Já subscrito no tópico '%s'.", mensagem->nome_topico);

            enviar_mensagem_manager(&resposta, manager_data);
        }
        else
        {
            MensagemManager resposta;
            resposta.tipo = MSG_SUBSCRICAO;
            snprintf(resposta.nome_utilizador, sizeof(resposta.nome_utilizador), "%s", mensagem->nome_utilizador);
            snprintf(resposta.nome_pipe, sizeof(resposta.nome_pipe), "%s", mensagem->nome_pipe);
            snprintf(resposta.nome_topico, sizeof(resposta.nome_topico), "%s", mensagem->nome_topico);
            snprintf(resposta.conteudo, sizeof(resposta.conteudo), "Inscrição no tópico '%s' realizada com sucesso.", mensagem->nome_topico);

            enviar_mensagem_manager(&resposta, manager_data);
        }
        int pipe_fd = open(mensagem->nome_pipe, O_WRONLY);
        if (pipe_fd == -1)
        {
            perror("Erro ao abrir pipe do novo subscritor");
        }
        else
        {
            for (int i = 0; i < MAX_MSGS_PERSISTENTES; i++)
            {
                if (manager_data->topicos[indice_topico].mensagens[i].ativa && manager_data->topicos[indice_topico].mensagens[i].tempo_vida > 0)
                {
                    MensagemManager msg_enviada = manager_data->topicos[indice_topico].mensagens[i].msg;
                    msg_enviada.tipo = MSG_PUBLICACAO;
                    write(pipe_fd, &msg_enviada, sizeof(MensagemManager));
                }
            }
            close(pipe_fd);
        }

        break;
    }
    // Mensagem de cancelamento de subscrição de um tópico
    // Remove o utilizador do tópico e envia uma mensagem de confirmação
    // Se o utilizador não estiver subscrito, envia uma mensagem de erro
    case MSG_DESUBSCRICAO:
    {
        printf("\nINFO: Utilizador '%s' cancelou subscrição do tópico '%s'.\n",
            mensagem->nome_utilizador, mensagem->nome_topico);

        int indice_topico = obter_ou_criar_topico(mensagem->nome_topico, manager_data);
        if (indice_topico == -1)
        {
            printf("Erro: Tópico '%s' não encontrado.\n", mensagem->nome_topico);

            MensagemManager resposta;
            resposta.tipo = MSG_DESUBSCRICAO;
            snprintf(resposta.nome_utilizador, sizeof(resposta.nome_utilizador), "%s", mensagem->nome_utilizador);
            snprintf(resposta.nome_topico, sizeof(resposta.nome_topico), "%s", mensagem->nome_topico);
            snprintf(resposta.conteudo, sizeof(resposta.conteudo), "Erro: Tópico '%s' não encontrado.", mensagem->nome_topico);

            enviar_mensagem_manager(&resposta, manager_data);
            break;
        }

        // Chamar a função que já lida com a lógica de subscrição e remoção de tópicos vazios
        cancelar_subscricao_topico_manager(mensagem->nome_topico, mensagem->nome_pipe, mensagem->nome_utilizador, manager_data);

        MensagemManager resposta;
        resposta.tipo = MSG_DESUBSCRICAO;
        snprintf(resposta.nome_utilizador, sizeof(resposta.nome_utilizador), "%s", mensagem->nome_utilizador);
        snprintf(resposta.nome_topico, sizeof(resposta.nome_topico), "%s", mensagem->nome_topico);
        snprintf(resposta.conteudo, sizeof(resposta.conteudo), "Cancelamento de subscrição do tópico '%s' realizado com sucesso.", mensagem->nome_topico);

        enviar_mensagem_manager(&resposta, manager_data);

        break;
    }
    // Mensagem de publicação de uma mensagem num tópico
    // Envia a mensagem para todos os subscritores do tópico
    // Se o tópico não existir, cria-o se possível
    // Se o utilizador não estiver subscrito, subscreve-o
    // Se o tópico estiver bloqueado, envia uma mensagem de erro
    // Se o tópico já tiver atingido o limite de mensagens persistentes, envia uma mensagem de erro
    // Se a mensagem tiver duração, guarda a mensagem num ficheiro
    // Se a mensagem tiver duração, guarda a mensagem na estrutura de mensagens persistentes
    case MSG_PUBLICACAO:
    {
        int indice_topico = -1;

        for (int i = 0; i < manager_data->num_topicos; i++)
        {
            if (strcmp(manager_data->topicos[i].nome_topico, mensagem->nome_topico) == 0)
            {
                indice_topico = i;
                break;
            }
        }
        // Se o tópico não existir, cria-o
        if (indice_topico == -1)
        {   
            indice_topico = obter_ou_criar_topico(mensagem->nome_topico, manager_data);
        
            if (indice_topico == -1)
            {
                printf("Erro: Limite de tópicos atingido.\n");
                return;
            }
        }

        if (manager_data->topicos[indice_topico].bloqueado)
        {
            printf("Erro: O tópico '%s' está bloqueado. Mensagem rejeitada.\n", mensagem->nome_topico);
            break;
        }
        if (manager_data->topicos[indice_topico].num_msgs_persistentes >= MAX_MSGS_PERSISTENTES)
        {
            printf("Erro: O tópico '%s' já atingiu o limite de mensagens persistentes.\n", mensagem->nome_topico);
            break;
        }
        printf("\nINFO: Utilizador '%s' publicou no tópico '%s': %s (Duração: %d).\n",
                   mensagem->nome_utilizador,
                   mensagem->nome_topico,
                   mensagem->conteudo,
                   mensagem->duracao);

        for (int i = 0; i < manager_data->topicos[indice_topico].num_subscritores; i++)
        {
            if (strcmp(manager_data->topicos[indice_topico].subscritores[i], mensagem->nome_pipe) == 0)
            {
                continue;
            }

            int pipe_fd = open(manager_data->topicos[indice_topico].subscritores[i], O_WRONLY);
            if (pipe_fd == -1)
            {
                perror("Erro ao abrir pipe do subscritor");
                continue;
            }

            MensagemManager mensagem_enviar = *mensagem;
            snprintf(mensagem_enviar.conteudo, sizeof(mensagem_enviar.conteudo),
                     "%s", mensagem->conteudo);

            write(pipe_fd, &mensagem_enviar, sizeof(MensagemManager));
            close(pipe_fd);
        }
        if (mensagem->duracao > 0)
        {

            for (int i = 0; i < MAX_MSGS_PERSISTENTES; i++)
            {
                if (!manager_data->topicos[indice_topico].mensagens[i].ativa)
                {
                    manager_data->topicos[indice_topico].mensagens[i].ativa = 1;
                    manager_data->topicos[indice_topico].mensagens[i].tempo_vida = mensagem->duracao;
                    manager_data->topicos[indice_topico].mensagens[i].msg = *mensagem;
                    manager_data->topicos[indice_topico].num_msgs_persistentes++;
                    break;
                }
            }
        }
        MensagemManager resposta;
        resposta.tipo = MSG_PUBLICACAO;
        snprintf(resposta.nome_utilizador, sizeof(resposta.nome_utilizador), "%s", mensagem->nome_utilizador);
        snprintf(resposta.nome_pipe, sizeof(resposta.nome_pipe), "%s", mensagem->nome_pipe);
        snprintf(resposta.nome_topico, sizeof(resposta.nome_topico), "%s", mensagem->nome_topico);
        snprintf(resposta.conteudo, sizeof(resposta.conteudo), "Mensagem publicada no tópico '%s' com sucesso.", mensagem->nome_topico);

        enviar_mensagem_manager(&resposta, manager_data);

        break;
    }

    // Mensagem de remoção de um utilizador
    case MSG_REMOVER:
    {
        printf("\nINFO: Utilizador '%s' pediu para ser removido.\n", mensagem->nome_utilizador);

        remover_utilizador(mensagem->nome_utilizador, manager_data);
        
        break;
    }
    // Mensagem para enviar todos os topicos disponíveis ao feed
    case MSG_TOPICOS:
    {   
        printf("\nINFO: Utilizador '%s' pediu para receber os topicos disponíveis.\n", mensagem->nome_utilizador);
        char lista_topicos[1024];
        obter_lista_topicos(lista_topicos, sizeof(lista_topicos), manager_data); // Função que obtém a lista de tópicos
        MensagemManager resposta;
        resposta.tipo = MSG_TOPICOS;
        snprintf(resposta.nome_utilizador, sizeof(resposta.nome_utilizador), "%s", mensagem->nome_utilizador);
        snprintf(resposta.nome_pipe, sizeof(resposta.nome_pipe), "%s", mensagem->nome_pipe);
        snprintf(resposta.conteudo, sizeof(resposta.conteudo), "%s", lista_topicos);
        enviar_mensagem_manager(&resposta, manager_data);
        break;
    }
    default:
        printf("\nAVISO: Tipo de mensagem desconhecido recebido.\n");
        break;
    }

    printf("\nDigite um comando (users, remove <user>, topics, show <topic>, lock <topic>, unlock <topic>, close): ");
}

// Função que obtém a lista de tópicos
// Se não existirem topicos criados mete isso no buffer e termina.
// Envia sempre o numero de topicos passiveis de criação
// Se não for possível criar novos topicos avisa isso mesmo
void obter_lista_topicos(char *buffer, size_t buffer_size, ManagerData *manager_data)
{
    buffer[0] = '\0'; 
    if (manager_data->num_topicos == 0)
    {
        snprintf(buffer, buffer_size, "INFO: Não há tópicos disponíveis.");
    }
    else
    {
        for (int i = 0; i < manager_data->num_topicos; i++)
        {
            char temp[100];
            snprintf(temp, sizeof(temp), 
                     "Tópico: %s | Estado: %s | Mensagens Persistentes: %d\n",
                     manager_data->topicos[i].nome_topico,
                     manager_data->topicos[i].bloqueado ? "Bloqueado" : "Desbloqueado",
                     manager_data->topicos[i].num_msgs_persistentes);

            // Adicionar ao buffer, respeitando o limite
            strncat(buffer, temp, buffer_size - strlen(buffer) - 1);
        }
    }
}

// Função para adicionar um utilizador à lista de utilizadores
// Adiciona o utilizador à lista de utilizadores
// Envia uma mensagem de boas-vindas ao utilizador
// Se o nome do utilizador já estiver em uso, envia uma mensagem de erro
// Se o número máximo de utilizadores for atingido, envia uma mensagem de erro
const char *adicionar_utilizador(const UtilizadorManager *utilizador, ManagerData *manager_data)
{
    pthread_mutex_lock(&manager_data->mutex_utilizadores);

    // Verificar se o nome do utilizador já está em uso
    for (int i = 0; i < manager_data->num_utilizadores; i++)
    {
        if (strcmp(manager_data->utilizadores_conectados[i].nome_utilizador, utilizador->nome_utilizador) == 0)
        {
            pthread_mutex_unlock(&manager_data->mutex_utilizadores);
            return "Erro: Nome de utilizador já está em uso.";
        }
    }

    // Adicionar novo utilizador
    if (manager_data->num_utilizadores < MAX_UTILIZADORES)
    {
        manager_data->utilizadores_conectados[manager_data->num_utilizadores++] = *utilizador;
        pthread_mutex_unlock(&manager_data->mutex_utilizadores);

        static char mensagem_boas_vindas[128];
        snprintf(mensagem_boas_vindas, sizeof(mensagem_boas_vindas), "Bem-vindo, %s!", utilizador->nome_utilizador);
        printf("O utilizador '%s' é bem vindo\n", utilizador->nome_utilizador);
        return mensagem_boas_vindas;
    }

    pthread_mutex_unlock(&manager_data->mutex_utilizadores);
    return "Erro: Número máximo de utilizadores atingido. Não é possível inscrever mais utilizadores.";
}

// Função para remover um utilizador da lista de utilizadores
// Envia uma mensagem de informação da remoção para todos os utilizadores
// Remove o utilizador da lista de utilizadores
// Cancela a subscrição do utilizador em todos os tópicos
// Envia uma mensagem de confirmação para o utilizador removido
void remover_utilizador(const char *nome_utilizador, ManagerData *manager_data)
{
    pthread_mutex_lock(&manager_data->mutex_utilizadores);

    int i = 0;
    while (i < manager_data->num_utilizadores)
    {
        if (strcmp(manager_data->utilizadores_conectados[i].nome_utilizador, nome_utilizador) == 0)
        {
            printf("\nINFO: A remover utilizador '%s' (pipe: '%s').\n",
                   manager_data->utilizadores_conectados[i].nome_utilizador,
                   manager_data->utilizadores_conectados[i].nome_pipe);

            MensagemManager mensagem;
            mensagem.tipo = MSG_REMOVER;
            snprintf(mensagem.nome_utilizador, sizeof(mensagem.nome_utilizador), "%s", nome_utilizador);
            snprintf(mensagem.nome_pipe, sizeof(mensagem.nome_pipe), "%s", manager_data->utilizadores_conectados[i].nome_pipe);

            int pipe_fd = open(manager_data->utilizadores_conectados[i].nome_pipe, O_WRONLY);
            if (pipe_fd != -1)
            {
                write(pipe_fd, &mensagem, sizeof(MensagemManager));
                close(pipe_fd);
            }

            for (int j = 0; j < manager_data->num_utilizadores; j++)
            {
                if (strcmp(manager_data->utilizadores_conectados[j].nome_utilizador, nome_utilizador) != 0)
                {
                    int pipe_fd = open(manager_data->utilizadores_conectados[j].nome_pipe, O_WRONLY);
                    if (pipe_fd != -1)
                    {
                        write(pipe_fd, &mensagem, sizeof(MensagemManager));
                        close(pipe_fd);
                    }
                }
            }

            for (int k = 0; k < manager_data->num_topicos; k++)
            {
                cancelar_subscricao_topico_manager(manager_data->topicos[k].nome_topico, manager_data->utilizadores_conectados[i].nome_pipe,manager_data->utilizadores_conectados[i].nome_utilizador, manager_data);
            }

            manager_data->utilizadores_conectados[i] = manager_data->utilizadores_conectados[--manager_data->num_utilizadores];
        }
        else
        {
            i++;
        }
    }

    pthread_mutex_unlock(&manager_data->mutex_utilizadores);
}

// Função para listar os utilizadores ativos
// Percorre a lista de utilizadores e imprime o nome e o pipe de cada utilizador
void listar_utilizadores(ManagerData *manager_data)
{
    printf("\nINFO: Utilizadores ativos (%d):\n", manager_data->num_utilizadores);
    for (int i = 0; i < manager_data->num_utilizadores; i++)
    {
        printf("- %s (Pipe: %s)\n", manager_data->utilizadores_conectados[i].nome_utilizador, manager_data->utilizadores_conectados[i].nome_pipe);
    }
}

// Função para listar os topicos ativos
// Percorre a lista de topicos e imprime o nome, o número de mensagens persistentes e o identificador dos subscritores de cada tópico
// Se o tópico não tiver subscritores, imprime uma mensagem a informar
// Se não houver tópicos, imprime uma mensagem a informar
void listar_topicos(ManagerData *manager_data)
{
    if (manager_data->num_topicos == 0)
    {
        printf("Não há tópicos disponíveis.\n");
        return;
    }

    printf("\nTópicos disponíveis:\n");
    for (int i = 0; i < manager_data->num_topicos; i++)
    {
        printf("\nTópico: %s\n", manager_data->topicos[i].nome_topico);
        printf("  Mensagens persistentes: %d\n", manager_data->topicos[i].num_msgs_persistentes);

        if (manager_data->topicos[i].num_subscritores == 0)
        {
            printf("\n  Nenhum utilizador subscrito.\n");
        }
        else
        {
            printf("\n  Subscritores: ");
            for (int j = 0; j < manager_data->topicos[i].num_subscritores; j++)
            {
                printf("%s", manager_data->topicos[i].subscritores_nome[j]);
                if (j < manager_data->topicos[i].num_subscritores - 1)
                {
                    printf(", ");
                }
            }
            printf("\n");
        }
    }
}

// Função para desubscrever um utilizador de um tópico
// Percorre a lista de tópicos e procura o tópico com o nome especificado
// Se o tópico não for encontrado, imprime uma mensagem de erro
// Se o utilizador não estiver subscrito, imprime uma mensagem de erro
// Se o utilizador estiver subscrito, remove o utilizador da lista de subscritores do tópico
// Se o tópico não tiver subscritores nem mensagens persistentes, remove o tópico
void cancelar_subscricao_topico_manager(const char *nome_topico,const char *nome_pipe, const char *nome_utilizador, ManagerData *manager_data)
{
    int indice_topico = -1;

    for (int i = 0; i < manager_data->num_topicos; i++)
    {
        if (strcmp(manager_data->topicos[i].nome_topico, nome_topico) == 0)
        {
            indice_topico = i;
            break;
        }
    }

    if (indice_topico == -1)
    {
        printf("\nErro: Tópico '%s' não encontrado.\n", nome_topico);
        return;
    }

    int encontrado = 0;
    for (int i = 0; i < manager_data->topicos[indice_topico].num_subscritores; i++)
    {
        if (strcmp(manager_data->topicos[indice_topico].subscritores_nome[i], nome_utilizador) == 0)
        {
            encontrado = 1;
            for (int j = i; j < manager_data->topicos[indice_topico].num_subscritores - 1; j++)
            {
                strcpy(manager_data->topicos[indice_topico].subscritores[j], manager_data->topicos[indice_topico].subscritores[j + 1]);
                strcpy(manager_data->topicos[indice_topico].subscritores_nome[j], manager_data->topicos[indice_topico].subscritores_nome[j + 1]);
            }
            manager_data->topicos[indice_topico].num_subscritores--;
            break;
        }
    }

    if (!encontrado)
    {
        printf("\nErro: Utilizador '%s' não está subscrito no tópico '%s'.\n", nome_utilizador, nome_topico);
        return;
    }

    printf("\nUtilizador '%s' removido do tópico '%s'.\n", nome_utilizador, nome_topico);

    if (manager_data->topicos[indice_topico].num_subscritores == 0 && manager_data->topicos[indice_topico].num_msgs_persistentes == 0)
    {
        printf("\nTópico '%s' não possui subscritores nem mensagens persistentes. A eliminá-lo.\n", nome_topico);

        for (int i = indice_topico; i < manager_data->num_topicos - 1; i++)
        {
            manager_data->topicos[i] = manager_data->topicos[i + 1];
        }
        manager_data->num_topicos--;
    }
}
// Função para mostrar as mensagens de um tópico
// Percorre a lista de tópicos e procura o tópico com o nome especificado
// Se o tópico não for encontrado, imprime uma mensagem de erro
// Se o tópico for encontrado, imprime as mensagens persistentes do tópico
// Se o tópico não tiver mensagens persistentes, imprime uma mensagem a informar
void mostrar_mensagens_topico(const char *nome_topico, ManagerData *manager_data)
{
    for (int i = 0; i < manager_data->num_topicos; i++)
    {
        if (strcmp(manager_data->topicos[i].nome_topico, nome_topico) == 0)
        {
            printf("\nMensagens do tópico '%s':\n", nome_topico);
            printf("  Mensagens persistentes: %d\n", manager_data->topicos[i].num_msgs_persistentes);

            int encontrou_mensagens = 0;

            for (int j = 0; j < MAX_MSGS_PERSISTENTES; j++)
            {
                if (manager_data->topicos[i].mensagens[j].ativa)
                {
                    encontrou_mensagens = 1;
                    printf("  - [%d segundos restantes] %s\n",
                           manager_data->topicos[i].mensagens[j].tempo_vida,
                           manager_data->topicos[i].mensagens[j].msg.conteudo);
                }
            }

            if (!encontrou_mensagens)
            {
                printf("  Não há mensagens ativas neste tópico.\n");
            }
            return;
        }
    }

    printf("\nErro: Tópico '%s' não encontrado.\n", nome_topico);
}

// Função para bloquear um tópico
// Percorre a lista de tópicos e procura o tópico com o nome especificado
// Se o tópico não for encontrado, imprime uma mensagem de erro
// Se o tópico for encontrado, bloqueia o tópico
// Envia uma mensagem de bloqueio para todos os subscritores do tópico
// Se o tópico estiver bloqueado, imprime uma mensagem de informação
void bloquear_topico(const char *nome_topico, ManagerData *manager_data) {
    int indice_topico = obter_ou_criar_topico(nome_topico, manager_data);
    if (indice_topico == -1) {
        printf("\nErro: Tópico '%s' não encontrado.\n", nome_topico);
        return;
    }
    manager_data->topicos[indice_topico].bloqueado = 1; 
    printf("\nINFO: O tópico '%s' foi bloqueado.\n", nome_topico);

    MensagemManager resposta;
    resposta.tipo = MSG_BLOQUEIO;
    snprintf(resposta.nome_topico, sizeof(resposta.nome_topico), "%s", nome_topico);
    snprintf(resposta.conteudo, sizeof(resposta.conteudo), "Tópico '%s' foi bloqueado", nome_topico);

    for (int i = 0; i < manager_data->topicos[indice_topico].num_subscritores; i++) {
        char *nome_pipe = manager_data->topicos[indice_topico].subscritores[i];
        snprintf(resposta.nome_pipe, sizeof(resposta.nome_pipe), "%s", nome_pipe);

        enviar_mensagem_manager(&resposta, manager_data);
    }
}

// Função para desbloquear um tópico
// Percorre a lista de tópicos e procura o tópico com o nome especificado
// Se o tópico não for encontrado, imprime uma mensagem de erro
// Se o tópico for encontrado, desbloqueia o tópico
// Envia uma mensagem de desbloqueio para todos os subscritores do tópico
// Se o tópico não estiver bloqueado, imprime uma mensagem de informação
void desbloquear_topico(const char *nome_topico, ManagerData *manager_data) {
    int indice_topico = obter_ou_criar_topico(nome_topico, manager_data);
    if (indice_topico == -1) {
        printf("\nErro: Tópico '%s' não encontrado.\n", nome_topico);
        return;
    }
    manager_data->topicos[indice_topico].bloqueado = 0; 
    printf("\nINFO: O tópico '%s' foi desbloqueado.\n", nome_topico);

    MensagemManager resposta;
    resposta.tipo = MSG_DESBLOQUEIO;
    snprintf(resposta.nome_topico, sizeof(resposta.nome_topico), "%s", nome_topico);
    snprintf(resposta.conteudo, sizeof(resposta.conteudo), "Tópico '%s' foi desbloqueado", nome_topico);

    for (int i = 0; i < manager_data->topicos[indice_topico].num_subscritores; i++) {
        snprintf(resposta.nome_pipe, sizeof(resposta.nome_pipe), "%s", manager_data->topicos[indice_topico].subscritores[i]);

        enviar_mensagem_manager(&resposta, manager_data);
    }
}

// Função para encerrar o manager
// Envia uma mensagem de remoção para todos os utilizadores
// Reescreve o ficheiro de mensagens
// Remove o pipe do manager
// Fecha o manager
void fechar_manager(ManagerData *manager_data)
{
    printf("A encerrar o manager e a notificar os utilizadores...\n");

    for (int i = 0; i < manager_data->num_utilizadores; i++)
    {
        MensagemManager mensagem;
        mensagem.tipo = MSG_REMOVER;
        snprintf(mensagem.nome_utilizador, sizeof(mensagem.nome_utilizador), "%s", manager_data->utilizadores_conectados[i].nome_utilizador);
        snprintf(mensagem.nome_pipe, sizeof(mensagem.nome_pipe), "%s", manager_data->utilizadores_conectados[i].nome_pipe);

        int pipe_fd = open(manager_data->utilizadores_conectados[i].nome_pipe, O_WRONLY);
        if (pipe_fd != -1)
        {
            write(pipe_fd, &mensagem, sizeof(MensagemManager));
            close(pipe_fd);
        }
    }
    reescrever_ficheiro_mensagens(manager_data);
    manager_data->num_utilizadores = 0;

    unlink(PIPE_MANAGER);
    exit(0);
}

// Função para obter o índice de um tópico
// Percorre a lista de tópicos e procura o tópico com o nome especificado
// Se o tópico for encontrado, retorna o índice do tópico
// Se o número máximo de tópicos for atingido, imprime uma mensagem de erro
// Se o topico não existir cria um novo tópico
int obter_ou_criar_topico(const char *nome_topico, ManagerData *manager_data)
{
    for (int i = 0; i < manager_data->num_topicos; i++)
    {
        if (strcmp(manager_data->topicos[i].nome_topico, nome_topico) == 0)
        {
            return i;
        }
    }

    if (manager_data->num_topicos >= MAX_TOPICOS)
    {
        printf("Erro: Limite máximo de tópicos atingido.\n");
        return -1;
    }

    snprintf(manager_data->topicos[manager_data->num_topicos].nome_topico, sizeof(manager_data->topicos[manager_data->num_topicos].nome_topico), "%s", nome_topico);
    manager_data->topicos[manager_data->num_topicos].num_msgs_persistentes = 0;
    manager_data->topicos[manager_data->num_topicos].bloqueado = 0;
    manager_data->topicos[manager_data->num_topicos].num_subscritores = 0;

    return manager_data->num_topicos++; 
}

// Função para adicionar um subscritor a um tópico
// Percorre a lista de tópicos e procura o tópico com o nome especificado
// Se o tópico não for encontrado, imprime uma mensagem de erro
// Se o utilizador já estiver subscrito, imprime uma mensagem de erro
// Se o tópico atingir o limite de subscritores, imprime uma mensagem de erro
// Adiciona o subscritor ao tópico
int adicionar_subscritor(const char *nome_topico, const char *nome_pipe, const char *nome_utilizador, ManagerData *manager_data)
{
    int indice_topico = -1;

    for (int i = 0; i < manager_data->num_topicos; i++)
    {
        if (strcmp(manager_data->topicos[i].nome_topico, nome_topico) == 0)
        {
            indice_topico = i;
            break;
        }
    }

    if (indice_topico == -1)
    {
        printf("Erro: Tópico '%s' não encontrado.\n", nome_topico);
        return 0; 
    }

    for (int i = 0; i < manager_data->topicos[indice_topico].num_subscritores; i++)
    {
        if (strcmp(manager_data->topicos[indice_topico].subscritores[i], nome_pipe) == 0)
        {
            printf("Utilizador '%s' já está subscrito no tópico '%s'.\n", nome_utilizador, nome_topico);
            return 0; 
        }
    }

    if (manager_data->topicos[indice_topico].num_subscritores >= MAX_UTILIZADORES)
    {
        printf("Erro: Limite de subscritores para o tópico '%s' atingido.\n", nome_topico);
        return 0; 
    }

    snprintf(manager_data->topicos[indice_topico].subscritores[manager_data->topicos[indice_topico].num_subscritores],
             TAM_MAX_NOME_UTILIZADOR, "%s", nome_pipe);
    snprintf(manager_data->topicos[indice_topico].subscritores_nome[manager_data->topicos[indice_topico].num_subscritores++],
             TAM_MAX_NOME_UTILIZADOR, "%s", nome_utilizador);

    printf("Utilizador '%s' subscrito no tópico '%s' com sucesso.\n", nome_utilizador, nome_topico);
    return 1; 
}


// Função para enviar uma mensagem para um cliente
// Percorre a lista de utilizadores conectados e procura o utilizador com o nome do pipe especificado
// Se o utilizador for encontrado, envia a mensagem para o pipe do cliente
// Se o utilizador não for encontrado, imprime uma mensagem de erro
void enviar_mensagem_manager(const MensagemManager *mensagem, ManagerData *manager_data)
{
    for (int i = 0; i < manager_data->num_utilizadores; i++)
    {
        if (strcmp(manager_data->utilizadores_conectados[i].nome_pipe, mensagem->nome_pipe) == 0)
        {
            int pipe_fd = open(manager_data->utilizadores_conectados[i].nome_pipe, O_WRONLY);
            if (pipe_fd == -1)
            {
                perror("Erro ao abrir o pipe do cliente para enviar a mensagem");
                return;
            }

            if (write(pipe_fd, mensagem, sizeof(MensagemManager)) == -1)
            {
                perror("Erro ao escrever no pipe do cliente");
            }

            close(pipe_fd);
            return; 
        }
    }

    printf("Erro: Não foi possível encontrar o utilizador '%s'.\n", mensagem->nome_utilizador);
}

// Função para tratar comandos do administrador
void tratar_comandos_admin(ManagerData *manager_data)
{
    char comando[50];
    printf("Digite um comando (users, remove <user>, topics, show <topic>, lock <topic>, unlock <topic>, close): ");
    scanf("%s", comando);
    // Se o comando for "users", lista os utilizadores ativos
    if (strcmp(comando, "users") == 0)
    {
        listar_utilizadores(manager_data);
    }
    // Se o comando for "remove", remove um utilizador
    else if (strcmp(comando, "remove") == 0)
    {
        char nome_utilizador[TAM_MAX_NOME_UTILIZADOR];
        scanf("%s", nome_utilizador);
        remover_utilizador(nome_utilizador, manager_data);
    }
    // Se o comando for "topics", lista os tópicos ativos
    else if (strcmp(comando, "topics") == 0)
    {
        listar_topicos(manager_data);
    }
    // Se o comando for "show", mostra as mensagens de um tópico
    else if (strcmp(comando, "show") == 0)
    {
        char topico[TAM_MAX_TOPICO];
        scanf("%s", topico);
        mostrar_mensagens_topico(topico, manager_data);
    }
    // Se o comando for "lock", bloqueia um tópico
    else if (strcmp(comando, "lock") == 0)
    {
        char topico[TAM_MAX_TOPICO];
        scanf("%s", topico);
        bloquear_topico(topico, manager_data);
    }
    // Se o comando for "unlock", desbloqueia um tópico
    else if (strcmp(comando, "unlock") == 0)
    {
        char topico[TAM_MAX_TOPICO];
        scanf("%s", topico);
        desbloquear_topico(topico, manager_data);
    }
    // Se o comando for "close", fecha o manager
    else if (strcmp(comando, "close") == 0)
    {
        fechar_manager(manager_data);
    }
    else
    {
        printf("Comando desconhecido.\n");
    }
}

// Função para obter mensagens de um ficheiro
// Abre o ficheiro de mensagens e lê as mensagens
// Caso existam mensagens persistentes, carrega as mensagens do ficheiro
// Cria os respetivos tópicos e guarda as mensagens persistentes
void carregar_mensagens_do_ficheiro(ManagerData *manager_data)
{
    const char *filename = obter_nome_ficheiro();
    FILE *ficheiro = fopen(filename, "r");
    if (!ficheiro)
    {
        printf("INFO: Nenhum ficheiro com mensagens persistentes encontrado.\n");
        return;
    }

    char linha[1024];
    while (fgets(linha, sizeof(linha), ficheiro))
    {
        char nome_topico[TAM_MAX_TOPICO];
        char nome_autor[TAM_MAX_NOME_UTILIZADOR];
        int duracao;
        char conteudo[TAM_MAX_MSG];

        if (sscanf(linha, "%s %s %d %[^\n]", nome_topico, nome_autor, &duracao, conteudo) == 4)
        {
            int indice_topico = obter_ou_criar_topico(nome_topico, manager_data);
            if (indice_topico == -1)
            {
                printf("Erro: Não foi possível criar o tópico '%s'.\n", nome_topico);
                continue;
            }

            for (int i = 0; i < MAX_MSGS_PERSISTENTES; i++)
            {
                if (!manager_data->topicos[indice_topico].mensagens[i].ativa)
                {
                    manager_data->topicos[indice_topico].mensagens[i].ativa = 1;
                    manager_data->topicos[indice_topico].mensagens[i].tempo_vida = duracao;
                    snprintf(manager_data->topicos[indice_topico].mensagens[i].msg.nome_utilizador, sizeof(nome_autor), "%s", nome_autor);
                    snprintf(manager_data->topicos[indice_topico].mensagens[i].msg.nome_topico, sizeof(nome_topico), "%s", nome_topico);
                    snprintf(manager_data->topicos[indice_topico].mensagens[i].msg.conteudo, sizeof(conteudo), "%s", conteudo);
                    manager_data->topicos[indice_topico].num_msgs_persistentes++;
                    break;
                }
            }
        }
    }

    fclose(ficheiro);
    printf("INFO: Mensagens persistentes carregadas do ficheiro.\n");
}

// Função para obter o nome do ficheiro de mensagens
// Verifica se a variável de ambiente MSG_FICH está definida
// Se não estiver definida, retorna o nome do ficheiro padrão
// Se estiver definida, retorna o nome do ficheiro
const char *obter_nome_ficheiro()
{
    const char *filename = getenv("MSG_FICH");
    if (filename == NULL || strlen(filename) == 0)
    {
        filename = DEFAULT_FILENAME;
    }
    return filename;
}

// Função para reescrever o ficheiro de mensagens
// Abre o ficheiro de mensagens para reescrita
// Escreve as mensagens ativas no ficheiro com os campos atualizados.
void reescrever_ficheiro_mensagens(ManagerData *manager_data)
{
    pthread_mutex_lock(&manager_data->mutex_mensagens); 
    const char *filename = obter_nome_ficheiro();
    FILE *ficheiro = fopen(filename, "w"); 

    if (!ficheiro)
    {
        perror("Erro ao abrir o ficheiro para reescrita");
        pthread_mutex_unlock(&manager_data->mutex_mensagens);
        return;
    }

    for (int i = 0; i < manager_data->num_topicos; i++)
    {
        for (int j = 0; j < MAX_MSGS_PERSISTENTES; j++)
        {
            if (manager_data->topicos[i].mensagens[j].ativa && manager_data->topicos[i].mensagens[j].tempo_vida > 0)
            {
                fprintf(ficheiro, "%s %s %d %s\n",
                        manager_data->topicos[i].nome_topico,
                        manager_data->topicos[i].mensagens[j].msg.nome_utilizador,
                        manager_data->topicos[i].mensagens[j].tempo_vida,
                        manager_data->topicos[i].mensagens[j].msg.conteudo);
            }
        }
    }
    printf("\nINFO: Mensagens persistentes reescritas no ficheiro: %s.\n", filename);
    fclose(ficheiro);
    pthread_mutex_unlock(&manager_data->mutex_mensagens); 
}

// Função para atualizar o tempo das mensagens
// Percorre a lista de tópicos e decrementa o tempo de vida das mensagens de 1 em 1 segundos
// Se o tempo de vida da mensagem for 0, imprime uma mensagem de informação
// Marca a mensagem como inativa
// Atualiza o número de mensagens persistentes do tópico
// Reescreve o ficheiro de mensagens
void atualizar_tempo_mensagens(ManagerData *manager_data)
{
    while (1)
    {
        pthread_mutex_lock(&manager_data->mutex_mensagens);

        for (int i = 0; i < manager_data->num_topicos; i++)
        {
            for (int j = 0; j < MAX_MSGS_PERSISTENTES; j++)
            {
                if (manager_data->topicos[i].mensagens[j].ativa && manager_data->topicos[i].mensagens[j].tempo_vida > 0)
                {
                    manager_data->topicos[i].mensagens[j].tempo_vida--; 

                    if (manager_data->topicos[i].mensagens[j].tempo_vida == 0)
                    {
                        printf("\nINFO: A mensagem '%s' do tópico '%s' expirou.\n",
                               manager_data->topicos[i].mensagens[j].msg.conteudo, manager_data->topicos[i].nome_topico);
                        manager_data->topicos[i].mensagens[j].ativa = 0;  
                        manager_data->topicos[i].num_msgs_persistentes--;
                    }
                }
            }
            // Verificar se o número de mensagens persistentes é zero assim como o número de subscritores
            if (manager_data->topicos[i].num_msgs_persistentes == 0 && manager_data->topicos[i].num_subscritores == 0)
            {
                printf("\nINFO: O tópico '%s' foi removido por não ter nem mensagens nem subscritores.\n", manager_data->topicos[i].nome_topico);

                for (int k = i; k < manager_data->num_topicos - 1; k++)
                {
                    manager_data->topicos[k] = manager_data->topicos[k + 1];
                }

                manager_data->num_topicos--;

                i--;
            }
        }

        pthread_mutex_unlock(&manager_data->mutex_mensagens); 

        sleep(1);
    }
}
