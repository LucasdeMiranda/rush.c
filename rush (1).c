 #include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAXLINHA 1024
#define READEND 0
#define WRITEEND 1

void lerlinha(char *linha);
void executar(char *linha);
void tratar_redirecionamento(char **cmd, char **arquivo_saida, char **arquivo_entrada);

int main() {
    char linha[MAXLINHA];
    do {
        printf("$ ");  // Prompt para o usuário digitar o comando
        lerlinha(linha);  // Lê a linha de comando digitada pelo usuário
        executar(linha);  // Executa o comando lido
    } while (1);  // Loop infinito para continuar aceitando comandos
    return 0;
}

// Função para ler a linha de comando do usuário
void lerlinha(char *linha) {
    if (fgets(linha, MAXLINHA, stdin) != NULL) {
        size_t len = strlen(linha);
        if (len > 0 && linha[len - 1] == '\n') {
            linha[len - 1] = '\0';  // Remove a nova linha no final da string adicionado pela função fgets
        }
    }
    if (strncmp(linha, "exit", 4) == 0) {  // Se o comando for "exit", termina o programa
        exit(0);
    }
}

// Função para tratar redirecionamento de entrada e saída
void tratar_redirecionamento(char **cmd, char **arquivo_saida, char **arquivo_entrada) {
    for (int i = 0; cmd[i] != NULL; i++) {
        //Verifica se o argumento atual é o símbolo de redirecionamento de saída (>).
        if (strcmp(cmd[i], ">") == 0) {
            *arquivo_saida = cmd[i + 1];  // Arquivo para onde a saída será redirecionada
            cmd[i] = NULL;  // Remove o símbolo de redirecionamento e o arquivo do comando para não dar erros 
        } else if (strcmp(cmd[i], "<") == 0) {
            *arquivo_entrada = cmd[i + 1];  // Arquivo de onde a entrada será redirecionada
            cmd[i] = NULL;  // Remove o símbolo de redirecionamento e o arquivo do comando
        }
    }
}

// Função para executar os comandos
void executar(char *linha) {
    char *comandos[MAXLINHA / 2 + 1];  // comandos é eu vertor de ponteiros para string o [MAXLINHA / 2 + 1] é uma estimativa segura porque na pior das hipoteses  
    //se tivermos uma linha com mais de 1024 caracteres ela pode ser dividida no maximo em 512 partes o +1 é para um ponteiro NULL (nulo)
    int i = 0;

    // Divide a linha de comando em comando separados por pipes
    comandos[i] = strtok(linha, "|");
    while (comandos[i] != NULL) {
        i++;
        comandos[i] = strtok(NULL, "|");
        //Continuar a partir da última posição onde strtok parou
    }

    int num_comandos = i;
    int descritorpipe[2 * (num_comandos - 1)]; // para n comandos é necessário n-1 exemplo 3 comandos 2 pipes  (pipes num_comandos - 1) * 2 cada pipe tem dois descritores de arquivos por isso é multiplicado por 2 
    pid_t pid; //representa o id dos processos

    // Cria pipes para comunicação entre os processos filhos
    for (i = 0; i < (num_comandos - 1); i++) {
        if (pipe(descritorpipe + i * 2) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Cria processos filhos para cada comando
    for (i = 0; i < num_comandos; i++) {
        if ((pid = fork()) == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) { // Código do processo filho
            if (i > 0) {
                // aqui temos o array descritorpipe  que contem os descritores de arquivos 
                // (i - 1) * 2: Calcula a posição base no array descritorpipe do pipe que conecta o comando anterior (i-1) ao comando atual (i).
                // + READEND: Adiciona o valor READEND (que é 0), para pegar o descritor de leitura do pipe.
                dup2(descritorpipe[(i - 1) * 2 + READEND], STDIN_FILENO);  // Redireciona a entrada padrão
            }

            if (i < (num_comandos - 1)) {
                dup2(descritorpipe[i * 2 + WRITEEND], STDOUT_FILENO);  // Redireciona a saída padrão
                //i * 2: Multiplica i por 2 para obter a posição base no array descritorpipe.
                // Cada pipe tem dois descritores (leitura e escrita), então multiplicamos por 2.
                //dup2 é usado para duplicar o descritor de arquivo descritorpipe[i * 2 + WRITEEND] no descritor STDOUT_FILENO,
                // que é o descritor de arquivo para a saída padrão.
            }

            // Fecha todos os descritores de pipe
            for (int j = 0; j < 2 * (num_comandos - 1); j++) {
                // esse -1 subtrai do número de comandos para ajustar o índice pipes  
                // ex:num_comandos = 3 (para cmd1 | cmd2 | cmd3), (3 comandos 2 pipes)
                close(descritorpipe[j]);
            }

            // Prepara os argumentos para execvp
            char *args[MAXLINHA / 2 + 1];
            // args é um array de ponteiros [MAXLINHA / 2 + 1] máximo de argumentos que se espera ter, lembrando que o +1 é para um ponteiro nulo (null). 

            int k = 0;
            //dividi a linha usando o delimitador espaço
            args[k] = strtok(comandos[i], " ");  
            while (args[k] != NULL) { 
                //Isso verifica se a última parte que você pegou não é NULL (vazia).
                k++;
                // k incrementa e avança para a próxima posição do array
                //Continuar a partir da última posição onde strtok parou
                args[k] = strtok(NULL, " "); 
            }

            // ponteiros para char null que vão guardar os nomes de arquivos especificados para o redirecionamentos de entrada 
            char *arquivo_saida = NULL;
            char *arquivo_entrada = NULL;
            tratar_redirecionamento(args, &arquivo_saida, &arquivo_entrada);

            // Trata redirecionamento de entrada
            if (arquivo_entrada != NULL) {
                //O_RDONLY: Abre o arquivo apenas para leitura  open é uma função
                int fd_in = open(arquivo_entrada, O_RDONLY);
                if (fd_in < 0) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                //Duplica o descritor de arquivo fd_in e o usa para substituir o descritor de entrada padrão (STDIN_FILENO, que é 0).
                // OU seja agora a entrada que seria lida pelo teclado  é lida pelo arquivo especificado 
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }

            // Trata redirecionamento de saída
            if (arquivo_saida != NULL) {
                // aqui utiliza os macros  O_WRONLY, O_CREAT  e O_TRUNC (macros são indentificadores definidos em c )
                // O_WRONLY significa que o arquivo vai ser aberto só para escrita 
                //O_CREAT significa que se o arquivo não existir ele será criado 
                //O_TRUNC se o arquivo foi criado ou já existir ele é zerado antes de escrever nele 
                // 0644 é uma permissão para o arquivo criado 
                // usa a função open para abrir o arquivo
                int fd_out = open(arquivo_saida, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0) {
                    //verifica se houve erro ao abrir o arquivo
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                // duplica o descritor de arquivo fd_out para a saída padrão 
                // Isso significa que qualquer saída escrita em STDOUT_FILENO será direcionada para o arquivo arquivo_saida.
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }

            // Executa o comando
            execvp(args[0], args);
            perror("execvp");  // Se execvp falhar
            exit(EXIT_FAILURE);
        }
    }

    // Fecha os descritores de pipe no processo pai
    for (i = 0; i < 2 * (num_comandos - 1); i++) {
        close(descritorpipe[i]);
    }

    // Espera todos os processos filhos terminarem
    for (i = 0; i < num_comandos; i++) {
        wait(NULL);
    }
}
