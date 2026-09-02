#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <omp.h>

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


int main(void){
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
    scanf("%d %d %d %d %d %d", &matriz_linhas, &matriz_colunas, &passos, &n_threads, &seed, &limiar_ignicao);
    printf("Os dados lidos foram: matriz_linhas: %d, matriz_colunas: %d, passos: %d, n_threads: %d, seed: %d, limiar_ignicao: %d\n", matriz_linhas, matriz_colunas, passos, n_threads, seed, limiar_ignicao);
    
    // Verificação de não negatividade. Caso tenha algum valor invalido, retorna imediatamente
    if (matriz_linhas <= 0 || matriz_colunas <= 0 || passos < 0 || n_threads <= 0 || limiar_ignicao < 0){
        printf("Dados inválidos para algum dos valores fornecidos\n");
        exit(1);
    }

    // Leitura da configuração do vento.
    // vento_linha
    // vento_coluna
    // vento_intensidade
    scanf("%d %d %d", &vento_linha, &vento_coluna, &intensidade_vento);
    printf("Os dados lidos foram: vento_linha: %d, vento_coluna: %d, intensidade_vento: %d\n", vento_linha, vento_coluna, intensidade_vento);
    
    // Verifica se os valores lidos estão no intervalo correto.
    if (vento_linha < -1 || vento_linha > 1 || vento_coluna < -1 || vento_coluna > 1 || (vento_coluna == 0 && vento_linha == 0) || intensidade_vento < 0 || intensidade_vento > 5){
        printf("Valores inválidos para a configuração do vento\n");
        exit(1);
    }

    // Leitura da quantidade de focos iniciais e zonas de contenção.
    // n_focos_iniciais
    // n_zonas_contencao
    scanf("%d %d",&n_focos_iniciais, &n_zonas_contencao);
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
        if (scanf("%d %d", &focos_iniciais[i].y, &focos_iniciais[i].x) != 2) {
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
        if (scanf("%d %d %d %d %d", &zonas_contencao[i].tempo_construcao, &zonas_contencao[i].p1.y, &zonas_contencao[i].p1.x, &zonas_contencao[i].p2.y, &zonas_contencao[i].p2.x) != 5) {
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
}