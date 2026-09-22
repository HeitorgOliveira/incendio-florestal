#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

// Da para otimizar o tamanho das variaveis, da para mudar o approach das matrizes para nao precisar copiar, utilizar arrays separados eh
// melhor pensando em cache e em SIMD

// DEFINES
#define true 1
#define false 0
#define verifica_gt_0(x) x > 0
#define verifica_gte_0(x) x >= 0
#define verifica_vento(x) x >= -1 && x <= 1
#define verifica_intensidade(x) x >= 0 && x <= 5
#define verifica_linha(x) x >= 0 && x < LINHA
#define verifica_coluna(x) x >= 0 && x < COLUNA

// CONSTANTES

const int DIRS[8][3] = { // Já botei o peso
            {1, 0, 10},
            {0, 1, 10},
            {1, 1, 7},
            {-1, 0, 10},
            {0, -1, 10},
            {1, -1, 7},
            {-1, 1, 7},
            {-1, -1, 7}
};

// ENUMS

typedef enum cobertura {
    AGUA, SOLO_EXPOSTO, VEGETACAO_RASTEIRA, FLORESTA
} cobertura;

typedef enum estado {
    NAO_COMBUSTIVEL, INTACTA, EM_CHAMAS, QUEIMADA, CONTENCAO
} estado;

typedef enum tempo_queima {
    TEMPO_NULO = 0,
    TEMPO_VEGETACAO = 2,
    TEMPO_FLORESTA = 4,
} tempo_queima;

typedef enum fator_queima {
    FATOR_NULO = 0,
    FATOR_VEGETACAO = 8,
    FATOR_FLORESTA = 12
} fator_queima;

// STRUCTS e Defines

typedef struct celula {
    cobertura cb;
    fator_queima fator;
    int umidade;
    estado es;
    tempo_queima tq;
    int tempo_ativacao; // -1 se nao fizer parte de uma zona, x caso contrario representando o menor valor de ativacao
} celula;


// VARIAVEIS
unsigned int SEED;
int LINHA, COLUNA, PASSOS, THREADS, LIMIAR; // HEADER; threads eh util nessa versão
int BORDA;
int VENTO_LINHA, VENTO_COLUNA, INTENSIDADE;// VENTO; 
int N_FOCOS, N_ZONAS;

celula *estado_atual, *prox_estado; // matrizes
int celulas_n_combustiveis = 0, celulas_intactas = 0, celulas_em_chamas = 0, celulas_queimadas = 0, celulas_de_contencao = 0;
int total_ignicoes = 0, passo_com_maior_numero_de_ignicoes = -1, qnt_ignicoes_passo_maior = 0, celulas_combustiveis_inicial = 0;
double percentual_queimado = 0, percentual_protegido = 0;
// FUNCOES

short int aberturaArquivo(FILE **fp, char* nomeArquivo) {
    *fp = fopen(nomeArquivo, "r");
    if (*fp == NULL) {
        return false;
    }
    return true;
}

cobertura avaliar_valor(int v) {
    if (v <= 9) return AGUA;
    if (v <= 19) return SOLO_EXPOSTO;
    if (v <= 54) return VEGETACAO_RASTEIRA;
    return FLORESTA;
}

void iniciar_matrizes(celula **matriz, celula **prox) {
    for(int i = 0; i < LINHA+2; i++) {
        for(int j = 0; j < COLUNA+2; j++) {
            int indice = (i * BORDA) + j;
            if (i == 0 || j == 0 || i == LINHA+1 || j == COLUNA+1){
                (*matriz)[indice].es = NAO_COMBUSTIVEL;
                (*prox)[indice].es = NAO_COMBUSTIVEL;
                continue;
            }

            cobertura c = avaliar_valor(rand_r(&SEED) % 100);
            int um = rand_r(&SEED) % 101;
            (*matriz)[indice].cb = c;
            (*prox)[indice].cb = c;

            (*matriz)[indice].umidade = um;
            (*prox)[indice].umidade = um;

            (*matriz)[indice].tempo_ativacao = -1;
            (*prox)[indice].tempo_ativacao = -1;

            (*matriz)[indice].tq = TEMPO_NULO;
            (*prox)[indice].tq = TEMPO_NULO;

            switch ((*matriz)[indice].cb) {
                case VEGETACAO_RASTEIRA:
                    (*matriz)[indice].fator = FATOR_VEGETACAO;
                    (*matriz)[indice].es = INTACTA;

                    (*prox)[indice].fator = FATOR_VEGETACAO;
                    (*prox)[indice].es = INTACTA;

                    celulas_combustiveis_inicial++;
                    break;

                case FLORESTA:
                    (*matriz)[indice].fator = FATOR_FLORESTA;
                    (*matriz)[indice].es = INTACTA;

                    (*prox)[indice].fator = FATOR_FLORESTA;
                    (*prox)[indice].es = INTACTA;

                    celulas_combustiveis_inicial++;
                    break;

                default:
                    (*matriz)[indice].fator = FATOR_NULO;
                    (*matriz)[indice].es = NAO_COMBUSTIVEL;

                    (*prox)[indice].fator = FATOR_NULO;
                    (*prox)[indice].es = NAO_COMBUSTIVEL;

                    celulas_n_combustiveis++;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        fprintf(stderr, "É necessário passar o nome do arquivo");
        return 1;
    } else if (argc > 2) {
        fprintf(stderr, "É necessário passar apenas o nome do arquivo");
        return 1;
    }

    FILE* fp;
    if (!aberturaArquivo(&fp, argv[1])){
        fprintf(stderr, "Houve um problema na abertura do arquivo");
        return 1;
    }

    fscanf(fp, " %d %d %d %d %u %d", &LINHA, &COLUNA, &PASSOS, &THREADS, &SEED, &LIMIAR); // Leitura Header
    if (!(verifica_gt_0(LINHA) && verifica_gt_0(COLUNA) && verifica_gte_0(PASSOS) && verifica_gt_0(THREADS) && verifica_gt_0(LIMIAR))) {
        fprintf(stderr, "Os parâmetros do header devem ser maiores do que 0");
        return 1;
    }

    fscanf(fp, " %d %d %d", &VENTO_LINHA, &VENTO_COLUNA, &INTENSIDADE); // Leitura Vento
    if (!(verifica_vento(VENTO_LINHA) && verifica_vento(VENTO_COLUNA) && !(VENTO_COLUNA == 0 && VENTO_LINHA == 0) && verifica_intensidade(INTENSIDADE))) {
        fprintf(stderr, "A direção do vento deve estar entre -1 e 1, ambos não podem ser 0 ao mesmo tempo e a intensidade deve estar entre 0 e 5");
        return 1;
    }

    fscanf(fp, " %d %d", &N_FOCOS, &N_ZONAS); // Leitura Focos e Zonas
    if(!(verifica_gte_0(N_FOCOS) && verifica_gte_0(N_ZONAS))) {
        fprintf(stderr, "O número de zonas e de focos deve ser maior do que 0");
        return 1;
    }

    // Criacao das matrizes
    BORDA = COLUNA + 2;
    estado_atual = malloc(sizeof(celula) * (LINHA+2) * BORDA);
    prox_estado = malloc(sizeof(celula) * (LINHA+2) * BORDA);

    iniciar_matrizes(&estado_atual, &prox_estado); // Talvez passar isso aqui para depois da leitura dos focos e zonas para fechar o arquivo mais cedo?

    // Focos iniciais 
    
    for (int i = 0; i < N_FOCOS; i++) {
        int linhaF, colunaF;
        fscanf(fp, " %d %d", &linhaF, &colunaF);
        if (!(verifica_linha(linhaF) && verifica_coluna(colunaF))) {
            fprintf(stderr, "O foco está localizado fora da matriz.");
            return 1;
        }
        int indice = ((linhaF + 1) * BORDA) + (colunaF+1);
        if (estado_atual[indice].es == NAO_COMBUSTIVEL) {
            fprintf(stderr, "O foco está localizado em um célula não combustível");
            return 1;
        }
        if (estado_atual[indice].es == EM_CHAMAS) {
            fprintf(stderr, "O foco está localizado em um célula que ja é foco inicial");
            return 1;
        }
        estado_atual[indice].es = EM_CHAMAS;
        prox_estado[indice].es = EM_CHAMAS;
        estado_atual[indice].tq = estado_atual[indice].cb == VEGETACAO_RASTEIRA ? TEMPO_VEGETACAO : TEMPO_FLORESTA;
        prox_estado[indice].tq = estado_atual[indice].tq;
        celulas_em_chamas++;
    }
    
    for(int i = 0; i < N_ZONAS; i++) {
        int timestamp, linhaIZ, colunaIZ, linhaFZ, colunaFZ;
        fscanf(fp, " %d %d %d %d %d", &timestamp, &linhaIZ, &colunaIZ, &linhaFZ, &colunaFZ);
        if (!(verifica_linha(linhaIZ) && verifica_linha(linhaFZ) && verifica_coluna(colunaIZ) && verifica_coluna(colunaFZ))) {
            fprintf(stderr, "A zona tem posições localizadas fora da matriz");
            return 1;

        }
        if ((linhaIZ > linhaFZ) || (colunaIZ > colunaFZ)) {
            fprintf(stderr, "Algum dos limites iniciais da zona são maiores do que os limites finais");
            return 1;
        }
        if (timestamp < 0 || timestamp >= PASSOS) {
            fprintf(stderr, "O passo de ativação não está em um intervalo válido");
            return 1;
        }
        for(int i = linhaIZ; i <= linhaFZ; i++) {
            for(int j = colunaIZ; j <= colunaFZ; j++) {
                int indice = ((i+1) * BORDA) + (j+1);
                if (estado_atual[indice].tempo_ativacao == -1 || estado_atual[indice].tempo_ativacao > timestamp) {
                    estado_atual[indice].tempo_ativacao = timestamp;
                    prox_estado[indice].tempo_ativacao = timestamp;
                }
            }
        }
    }

    fclose(fp); // fim da leitura

    celulas_intactas = celulas_combustiveis_inicial - celulas_em_chamas;
    int tempo_atual = 0;

    int qnt_ignicoes = 0;
    int continuar = true;

    omp_set_dynamic(0);
    double tempo_inicial = omp_get_wtime();

    #pragma omp parallel num_threads(THREADS) default(none) \
        shared(estado_atual, prox_estado, tempo_atual, qnt_ignicoes, continuar, \
               celulas_intactas, celulas_em_chamas, celulas_queimadas, \
               celulas_de_contencao, total_ignicoes, qnt_ignicoes_passo_maior, \
               passo_com_maior_numero_de_ignicoes, PASSOS, LINHA, COLUNA, BORDA, \
               DIRS, VENTO_LINHA, VENTO_COLUNA, INTENSIDADE, LIMIAR)
    {
        while(true) {
            #pragma omp single
            {
                continuar = (tempo_atual < PASSOS && celulas_em_chamas);
                qnt_ignicoes = 0;
            }

            if (!continuar) break;

            // Ativar as zonas
            #pragma omp for collapse(2) schedule(static) \
            reduction(+:celulas_intactas, celulas_de_contencao) // trocar o static por dynamic para fazer as comparacoes lendarias
            for(int i = 0; i < LINHA; i++) {
                for(int j = 0; j < COLUNA; j++) {
                    int indice = ((i+1) * BORDA) + (j+1);
                    if (estado_atual[indice].tempo_ativacao == tempo_atual && estado_atual[indice].es == INTACTA) {
                        celulas_intactas--;
                        celulas_de_contencao++;
                        prox_estado[indice].es = CONTENCAO;
                    }
                }
            }

            #pragma omp for collapse(2) schedule(static) \
                reduction(+:celulas_intactas, celulas_em_chamas, celulas_queimadas, total_ignicoes, qnt_ignicoes)
            for(int i = 0; i < LINHA; i++) {
                for(int j = 0; j < COLUNA; j++) {
                    int indice = ((i+1) * BORDA) + (j+1);

                    if (estado_atual[indice].tempo_ativacao == tempo_atual && estado_atual[indice].es == INTACTA) {
                        continue;
                    }
                    prox_estado[indice] = estado_atual[indice];

                    if (estado_atual[indice].es == INTACTA) { // Prox estado
                        int peso_total = 0;
                        #pragma omp simd reduction(+:peso_total)
                        for(int k = 0; k < 8; k++) {
                            int iatual = i + 1, jatual = j + 1;

                            iatual += DIRS[k][0];
                            jatual += DIRS[k][1];

                            int es_EM_CHAMAS = estado_atual[(iatual * BORDA) + jatual].es == EM_CHAMAS;

                            char alinhamento_vento = (VENTO_LINHA * (-DIRS[k][0])) + (VENTO_COLUNA * (-DIRS[k][1]));

                            int peso_vizinho = DIRS[k][2] + (INTENSIDADE * alinhamento_vento);
                            peso_total += (peso_vizinho > 1 ? peso_vizinho : 1) * es_EM_CHAMAS;

                        }

                        int potencial = (peso_total * estado_atual[indice].fator * (100 - estado_atual[indice].umidade))/(100);
                        if (potencial >= LIMIAR) {
                            prox_estado[indice].es = EM_CHAMAS;
                            prox_estado[indice].tq = estado_atual[indice].cb == VEGETACAO_RASTEIRA ? TEMPO_VEGETACAO : TEMPO_FLORESTA;
                            celulas_em_chamas++;
                            celulas_intactas--;
                            total_ignicoes++;
                            qnt_ignicoes++;
                        }
                        
                    } else if (estado_atual[indice].es == EM_CHAMAS) {
                        prox_estado[indice].tq = estado_atual[indice].tq - 1;
                        if (prox_estado[indice].tq == 0) {
                            prox_estado[indice].es = QUEIMADA;
                            celulas_em_chamas--;
                            celulas_queimadas++;
                        }
                    }
                    
                }
            }

            #pragma omp single
            {
                if (qnt_ignicoes > qnt_ignicoes_passo_maior) {
                    qnt_ignicoes_passo_maior = qnt_ignicoes;
                    passo_com_maior_numero_de_ignicoes = tempo_atual;
                }
            }

            #pragma omp single
            { // Da para usar simd aqui caso eu troque para copia de matrizes (ver se compensa)
                celula *aux = estado_atual;
                estado_atual = prox_estado; 
                prox_estado = aux;
                tempo_atual++;
            }
        }
    }

    double tempo_final = omp_get_wtime();

    if (celulas_combustiveis_inicial == 0) {
        percentual_queimado = 0.0;
        percentual_protegido = 0.0;
    }
    else {
        percentual_queimado = 100 * ((celulas_queimadas + celulas_em_chamas)/(double)celulas_combustiveis_inicial);
        percentual_protegido = 100 * (celulas_de_contencao/(double)celulas_combustiveis_inicial);
    }

    unsigned long long checksum = 0;
    for(long long k = 0; k < LINHA*COLUNA; k++) { // Vou mudar todos os for para isso, que loucura boa/ tem que verificar se eh melhor pro omp, mas acho que sim

        long long i = k / COLUNA;
        long long j = k % COLUNA;
        long long indice = ((i+1) * BORDA) + (j+1);

        checksum = checksum * 31ULL + (unsigned long long)estado_atual[indice].es;
        checksum = checksum * 31ULL + (unsigned long long)estado_atual[indice].tq;
    }

    printf("passos: %d\nnao_combustiveis: %d\nintactas: %d\nem_chamas: %d\nqueimadas: %d\ncontencao: %d\ntotal_ignicoes: %d\npico_ignicoes: %d %d\npercentual_queimado:\
 %.2f\npercentual_protegido: %.2f\nchecksum: %llu\ntempo: %f", tempo_atual, celulas_n_combustiveis, celulas_intactas, celulas_em_chamas, celulas_queimadas,\
              celulas_de_contencao, total_ignicoes, passo_com_maior_numero_de_ignicoes, qnt_ignicoes_passo_maior, percentual_queimado,  percentual_protegido, checksum, \
              tempo_final - tempo_inicial);

    free(estado_atual);
    free(prox_estado);
    return 0;
}
