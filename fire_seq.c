#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <omp.h>

#define AGUA 0
#define SOLO_EXPOSTO 0
#define VEGETACAO 8
#define FLORESTA 12

// ------------------------- STRUCTS -------------------------


// Vento
typedef struct {
    int linha;
    int coluna;
    float intensidade;
} vento;

// Define ponto como o par de inteiros x e y
typedef struct {
    int x;
    int y;
} ponto;

// Define como zona de contenção o tempo de construção e o intervalo dentro de dois pontos
// O tempo de construção é o tempo em que a área determinada pela zona de contenção passará a fazer efeito
typedef struct {
    int tempo_construcao;
    ponto p1;
    ponto p2;
} zona_contencao;

// ------------------------- Funções -------------------------

// Função para retornar a direção do vento dado um par ordenado (linha x coluna):
char *direcao_vento(int linha, int coluna) {
    switch(linha * 10 + coluna) {
        case -10 + 0: return "Norte";
        case -10 + 1: return "Nordeste";
        case 0 + 1: return "Leste";
        case 10 + 1: return "Sudeste";
        case 10 + 0: return "Sul";
        case 10 -1: return "Sudoeste";
        case 0 - 1: return "Oeste";
        case -10 - 1: return "Noroeste";
        default: return "Invalido";
    }
}

int resgata_codigo(unsigned int *seed) {
    int valor = rand_r(seed) % 100;
    if (valor >= 0 && valor <= 9) return 0;
    if (valor >=10 && valor <= 19) return 1;
    if (valor >= 20 && valor <= 54) return 2;
    if (valor >= 55 && valor <= 99) return 3;
    return -1;
}

void geracao_cobertura(unsigned int seed, int linhas, int colunas, int **matriz) {
    for (int i = 0; i < linhas; i++) {
        for (int j = 0; j < colunas; j++) {
            // Geração de cobertura com base na semente
            int codigo = resgata_codigo(&seed);
            matriz[i][j] = codigo;
        }
    }
}

void exibe_matriz(int linhas, int colunas, int **matriz) {
    for (int i = 0; i < linhas; i ++) {
        for (int j = 0; j < colunas; j++) {
            printf("%d ", matriz[i][j]);
        }
        printf("\n");
    }
}

int main(int argc, char *argv[]){
    // Verificar se os argumentos estão corretos:
    if (argc != 2) {
        printf("Uso: %s <arquivo.txt>\n", argv[0]);
        exit(1);
    }

    FILE *arquivo = fopen(argv[1], "r");
    if (!arquivo) {
        printf("Erro ao abrir o arquivo: %s\n", argv[1]);
        exit(1);
    }

    // Declaração de variáveis
    int matriz_linhas, matriz_colunas;
    int passos;
    int n_threads;
    int seed;
    int limiar_ignicao;
    int vento_linha, vento_coluna;
    int intensidade_vento;
    int n_focos_iniciais, n_zonas_contencao;
    
    // Leitura e debug dos dados, os parênteses exibem o nome das variáveis no PDF:
    // matriz_linhas (L)
    // matriz_colunas (C)
    // passos (P)
    // n_threads (T)
    // seed
    // limiar_ignicao (LIMIAR)
    fscanf(arquivo, "%d %d %d %d %d %d", &matriz_linhas, &matriz_colunas, &passos, &n_threads, &seed, &limiar_ignicao);
    printf("Os dados lidos foram: matriz_linhas: %d, matriz_colunas: %d, passos: %d, n_threads: %d, seed: %d, limiar_ignicao: %d\n", matriz_linhas, matriz_colunas, passos, n_threads, seed, limiar_ignicao);

    // Verificação de não negatividade. Caso tenha algum valor invalido, retorna imediatamente
    if (matriz_linhas <= 0 || matriz_colunas <= 0 || passos < 0 || n_threads <= 0 || limiar_ignicao < 0){
        printf("Dados inválidos para algum dos valores fornecidos\n");
        exit(1);
    }

    int **matriz = malloc(sizeof(int *) * matriz_linhas);
    if (matriz == NULL) {
        printf("Erro ao alocar memória para a matriz\n");
        exit(1);
    }
    for (int i = 0; i < matriz_linhas; i++) {
        matriz[i] = malloc(sizeof(int) * matriz_colunas);
        if (matriz[i] == NULL) {
            printf("Erro ao alocar memória para a linha %d\n", i);
            exit(1);
        }
    }

    // Leitura da configuração do vento.
    // vento_linha
    // vento_coluna
    // vento_intensidade
    fscanf(arquivo, "%d %d %d", &vento_linha, &vento_coluna, &intensidade_vento);
    printf("Os dados lidos foram: vento_linha: %d, vento_coluna: %d, intensidade_vento: %d\n", vento_linha, vento_coluna, intensidade_vento);

    // Verifica se os valores lidos estão no intervalo correto.
    if (vento_linha < -1 || vento_linha > 1 || vento_coluna < -1 || vento_coluna > 1 || (vento_coluna == 0 && vento_linha == 0) || intensidade_vento < 0 || intensidade_vento > 5){
        printf("Valores inválidos para a configuração do vento\n");
        exit(1);
    }

    // Leitura da quantidade de focos iniciais e zonas de contenção.
    // n_focos_iniciais
    // n_zonas_contencao
    fscanf(arquivo, "%d %d", &n_focos_iniciais, &n_zonas_contencao);
    printf("Os dados lidos foram: focos_iniciais: %d, n_zonas_contencao: %d\n", n_focos_iniciais, n_zonas_contencao);

    // Verifica se os valores são validos.
    if (n_focos_iniciais < 0 || n_zonas_contencao < 0){
        printf("Valores inválidos para numero de focos iniciais ou zonas de contenção\n");
        exit(1);
    }

    // Inicializa o vetor que vai conter os pontos de focos iniciais de incêndio
    ponto *focos_iniciais = malloc(sizeof(ponto) * n_focos_iniciais);
    if (!focos_iniciais){
        printf("Erro alocando memória.\n");
        exit(1);
    }

    // Adiciona os focos iniciais de incêndio.
    for (int i = 0; i < n_focos_iniciais; i++){
        // Adicionar os focos iniciais de incêndio.

        // Verifica se os valores são números.
        if (fscanf(arquivo, "%d %d", &focos_iniciais[i].y, &focos_iniciais[i].x) != 2) {
            printf("Erro na leitura das coordenadas\n");
            free(focos_iniciais);
            exit(1);
        } 

        // Verifica se os valores adicionados estão dentro da matriz
        if (focos_iniciais[i].x < 0 || focos_iniciais[i].x >= matriz_colunas || focos_iniciais[i].y < 0 || focos_iniciais[i].y >= matriz_linhas){
            printf("Valor inválido (fora da matriz fornecida)\n");
            free(focos_iniciais);
            exit(1);
        }
    }

    // Adiciona os pontos das zonas iniciais de contenção
    zona_contencao *zonas_contencao = malloc(sizeof(zona_contencao) * n_zonas_contencao);
    if (!zonas_contencao) {
        printf("Erro alocando memória.\n");
        free(focos_iniciais);
        exit(1);
    }

    // Adiciona as zonas iniciais de contenção
    for (int i = 0; i < n_zonas_contencao; i++) {
        // Adicionar zonas de contenção
        if (fscanf(arquivo, "%d %d %d %d %d", &zonas_contencao[i].tempo_construcao, &zonas_contencao[i].p1.y, &zonas_contencao[i].p1.x, &zonas_contencao[i].p2.y, &zonas_contencao[i].p2.x) != 5) {
            printf("Erro na leitura das coordenadas\n");
            free(focos_iniciais);
            free(zonas_contencao);
            exit(1);
        }

        // Verifica se os valores adicionados estão dentro da matriz
        if (zonas_contencao[i].p1.x < 0 || zonas_contencao[i].p1.x >= matriz_colunas || zonas_contencao[i].p1.y < 0 || zonas_contencao[i].p1.y >= matriz_linhas ||
            zonas_contencao[i].p2.x < 0 || zonas_contencao[i].p2.x >= matriz_colunas || zonas_contencao[i].p2.y < 0 || zonas_contencao[i].p2.y >= matriz_linhas) {
            printf("Valor inválido (fora da matriz fornecida)\n");
            free(focos_iniciais);
            free(zonas_contencao);
            exit(1);
        }
    }

    printf("TOTAL DE FOCOS DE INCENDIO: \n");
    for (int i = 0; i < n_focos_iniciais; i++) {
        printf("Foco %d: (%d, %d)\n", i + 1, focos_iniciais[i].x, focos_iniciais[i].y);
    }
    printf("TOTAL DE ZONAS DE CONTENÇÃO: \n");
    for (int i = 0; i < n_zonas_contencao; i++) {
        printf("Zona %d: Tempo de construção: %d, P1: (%d, %d), P2: (%d, %d)\n", i + 1, zonas_contencao[i].tempo_construcao, zonas_contencao[i].p1.x, zonas_contencao[i].p1.y, zonas_contencao[i].p2.x, zonas_contencao[i].p2.y);
    }

    geracao_cobertura(seed, matriz_linhas, matriz_colunas, matriz);
    exibe_matriz(matriz_linhas, matriz_colunas, matriz);

    fclose(arquivo);
    for (int i = 0; i < matriz_linhas; i++) {
        free(matriz[i]);
    }
    free(matriz);
}