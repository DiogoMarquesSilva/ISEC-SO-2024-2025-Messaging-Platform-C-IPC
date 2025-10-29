
#ifndef PLATAFORMA_MENSAGENS_H
#define PLATAFORMA_MENSAGENS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <unistd.h> 
#include <fcntl.h>  
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>

// Definições
// Pipe do manager
#define PIPE_MANAGER "pipe_manager" 
// Número máximo de utilizadores
#define MAX_UTILIZADORES 10
// Número máximo de tópicos
#define MAX_TOPICOS 20
// Número máximo de mensagens persistentes por tópico
#define MAX_MSGS_PERSISTENTES 5
// Tamanho máximo do nome de um tópico
#define TAM_MAX_TOPICO 20
// Tamanho máximo do conteúdo de uma mensagem
#define TAM_MAX_MSG 300
// Tamanho máximo do nome de um utilizador
#define TAM_MAX_NOME_UTILIZADOR 20
// Número máximo de tópicos subscritos
#define MAX_TOPICOS_SUBSCRITOS MAX_TOPICOS
// Número máximo de mensagens persistentes
#define DEFAULT_FILENAME "mensagens_persistentes.txt"


// Estruturas

// Tipos de mensagens
typedef enum
{
    MSG_INSCRICAO,   
    MSG_SUBSCRICAO,  
    MSG_PUBLICACAO, 
    MSG_REMOVER,    
    MSG_DESUBSCRICAO,
    MSG_BLOQUEIO,    
    MSG_DESBLOQUEIO,
    MSG_TOPICOS  

} TipoMensagem;

// Estrutura de uma mensagem
typedef struct
{
    TipoMensagem tipo;                            
    char nome_utilizador[TAM_MAX_NOME_UTILIZADOR]; 
    char nome_topico[TAM_MAX_TOPICO];          
    char conteudo[TAM_MAX_MSG];                 
    int duracao;                                
    char nome_pipe[50];                         
} MensagemManager;

// Estrutura de um tópico no feed
typedef struct
{
    char nome_topico[TAM_MAX_TOPICO];
    int subscrito; 
} Topico;

// Estrutura de um utilizador
typedef struct
{
    char nome_utilizador[TAM_MAX_NOME_UTILIZADOR]; 
    char nome_pipe[50];                      
    Topico topicos[MAX_TOPICOS];           
    int num_topicos;                         
} UtilizadorManager;

// Estrutura de uma mensagem persistente
typedef struct
{
    MensagemManager msg;
    int ativa;     
    int tempo_vida; 

} MensagemPersistente;

// Estrutura de um tópico no manager
typedef struct
{
    char nome_topico[TAM_MAX_TOPICO];
    int num_msgs_persistentes;
    int bloqueado;
    char subscritores[MAX_UTILIZADORES][TAM_MAX_NOME_UTILIZADOR];
    char subscritores_nome[MAX_UTILIZADORES][TAM_MAX_NOME_UTILIZADOR];
    int num_subscritores;
    MensagemPersistente mensagens[MAX_MSGS_PERSISTENTES];
} TopicoManager;

typedef struct {
    Topico topicos_subscritos[MAX_TOPICOS_SUBSCRITOS];
    int num_topicos_subscritos;
    char pipe_cliente[50];
    int bem_vindo_recebido;
    int inscricao_confirmada;
    pthread_t thread_comandos;
} FeedData;

typedef struct {
    UtilizadorManager utilizadores_conectados[MAX_UTILIZADORES];
    int num_utilizadores;
    pthread_mutex_t mutex_utilizadores;
    pthread_mutex_t mutex_mensagens;
    TopicoManager topicos[MAX_TOPICOS];
    int num_topicos;
} ManagerData;

// Prototipos de funções
// Funções do manager
void *comandos_admin_thread(void *arg);// Função para tratar os comandos do administrador
void tratar_comandos_admin();//Função para tratar os comandos do administrador
void listar_utilizadores(); //Função para listar os utilizadores
void fechar_manager(); //Função para fechar o manager 
void carregar_mensagens_do_ficheiro();// Função para carregar mensagens do ficheiro
const char *obter_nome_ficheiro(); //Função para obter o nome do ficheiro
void reescrever_ficheiro_mensagens(); //Função para reescrever o ficheiro de mensagens
void atualizar_tempo_mensagens(); //Função para atualizar o tempo das mensagens
void iniciar_manager(ManagerData *manager_data);//Função para iniciar o manager
void tratar_mensagem(const MensagemManager *mensagem, ManagerData *manager_data);//Função para tratar uma mensagem
bool verificar_inscricao_topico(const char *nome_pipe, const char *nome_topico, ManagerData *manager_data);//Função para verificar a inscrição num tópico
void obter_lista_topicos(char *buffer, size_t buffer_size, ManagerData *manager_data);//Função para obter a lista de tópicos
const char *adicionar_utilizador(const UtilizadorManager *utilizador, ManagerData *manager_data);//Função para adicionar um utilizador
void remover_utilizador(const char *nome_utilizador, ManagerData *manager_data);//Função para remover um utilizador
void listar_utilizadores(ManagerData *manager_data);//Função para listar os utilizadores
void listar_topicos(ManagerData *manager_data);//Função para listar os tópicos
void cancelar_subscricao_topico_manager(const char *nome_topico,const char *nome_pipe, const char *nome_utilizador, ManagerData *manager_data);//Função para cancelar a subscrição de um tópico
void mostrar_mensagens_topico(const char *nome_topico, ManagerData *manager_data);//Função para mostrar as mensagens de um tópico
void bloquear_topico(const char *nome_topico, ManagerData *manager_data);//Função para bloquear um tópico
void desbloquear_topico(const char *nome_topico, ManagerData *manager_data);//Função para desbloquear um tópico
void fechar_manager(ManagerData *manager_data);//Função para fechar o manager
int obter_ou_criar_topico(const char *nome_topico, ManagerData *manager_data);//Função para obter ou criar um tópico
int adicionar_subscritor(const char *nome_topico, const char *nome_pipe, const char *nome_utilizador, ManagerData *manager_data);//Função para adicionar um subscritor
void enviar_mensagem_manager(const MensagemManager *mensagem, ManagerData *manager_data);//Função para enviar uma mensagem ao manager
void tratar_comandos_admin(ManagerData *manager_data);//Função para tratar os comandos do administrador
void carregar_mensagens_do_ficheiro(ManagerData *manager_data);//Função para carregar mensagens do ficheiro
const char *obter_nome_ficheiro();//Função para obter o nome do ficheiro
void reescrever_ficheiro_mensagens(ManagerData *manager_data);//Função para reescrever o ficheiro de mensagens
void atualizar_tempo_mensagens(ManagerData *manager_data);//Função para atualizar o tempo das mensagens

// Funções do feed
void tratar_comandos_utilizador();// Função para tratar os comandos do utilizador
void listar_comandos(); //Função para listar os comandos disponíveis
void listar_topicos_subscritos(); //Função para listar os tópicos subscritos
void enviar_mensagem(const MensagemManager *mensagem); //Função para enviar uma mensagem ao manager
void sair_feed(); //Função para sair do feed
void *thread_receber_mensagens(void *arg);//Função para receber mensagens
void iniciar_feed(const char *nome_utilizador, FeedData *feed_data);////Função para iniciar o feed
void tratar_comandos_utilizador(const char *nome_utilizador, FeedData *feed_data);////Função para tratar os comandos do utilizador
void listar_comandos();//Função para listar os comandos disponíveis
void listar_topicos_subscritos(FeedData *feed_data);//Função para listar os tópicos subscritos
void adicionar_topico(const char *nome_topico, FeedData *feed_data);//Função para adicionar um tópico
int verificar_inscricao(const char *nome_topico, FeedData *feed_data);//Função para verificar a inscrição num tópico
void cancelar_subscricao_topico(const char *nome_topico, FeedData *feed_data);//Função para cancelar a subscrição de um tópico
void signal_handler(int signal);
#endif