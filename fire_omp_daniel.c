#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

/* defines */
#define true 1
#define false 0
#define verifica_gt_0(x) (x > 0)
#define verifica_gte_0(x) (x >= 0)
#define verifica_vento(x) (x >= -1 && x <= 1)
#define verifica_intensidade(x) (x >= 0 && x <= 5)
#define verifica_linha(x) (x >= 0 && x < LINHA)
#define verifica_coluna(x) (x >= 0 && x < COLUNA)

/* enums */
typedef enum cobertura { AGUA, SOLO_EXPOSTO, VEGETACAO_RASTEIRA, FLORESTA } cobertura;
typedef enum estado { NAO_COMBUSTIVEL, INTACTA, EM_CHAMAS, QUEIMADA, CONTENCAO } estado;
typedef enum tempo_queima { TEMPO_NULO = 0, TEMPO_VEGETACAO = 2, TEMPO_FLORESTA = 4 } tempo_queima;
typedef enum fator_queima { FATOR_NULO = 0, FATOR_VEGETACAO = 8, FATOR_FLORESTA = 12 } fator_queima;

/* arrays de atributos, com borda sentinela: dimensão (LINHA+2) x (COLUNA+2) */
estado *estados_atuais;
estado *estados_prox;
int *umidades;
fator_queima *fatores;
tempo_queima *tempos_queima_atuais;
tempo_queima *tempos_queima_prox;
int *tempos_ativacao;

// constantes
int LINHA, COLUNA, PASSOS, THREADS, LIMIAR;
int BORDA; // = COLUNA + 2
int VENTO_LINHA, VENTO_COLUNA, INTENSIDADE;
int N_FOCOS, N_ZONAS;
unsigned int SEED;

const int DIRS[8][3] = {
            {1, 0, 10},
            {0, 1, 10},
            {1, 1, 7},
            {-1, 0, 10},
            {0, -1, 10},
            {1, -1, 7},
            {-1, 1, 7},
            {-1, -1, 7}
};

// métricas gerais variáveis
int celulas_n_combustiveis = 0, celulas_combustiveis_inicial = 0;
int celulas_intactas = 0, celulas_em_chamas = 0, celulas_queimadas = 0, celulas_de_contencao = 0;
int total_ignicoes = 0, passo_com_maior_numero_de_ignicoes = -1, qnt_ignicoes_passo_maior = 0;
double percentual_queimado = 0, percentual_protegido = 0;

short int aberturaArquivo(FILE **fp, char* nomeArquivo) {
    *fp = fopen(nomeArquivo, "r");
    return (*fp != NULL);
}

cobertura avaliar_valor(int v) {
    if (v <= 9) return AGUA;
    if (v <= 19) return SOLO_EXPOSTO;
    if (v <= 54) return VEGETACAO_RASTEIRA;
    return FLORESTA;
}

// geração da cobertura é inerentemente sequencial (rand_r encadeado célula a célula
// na ordem exigida pelo enunciado), então não é paralelizada
void iniciar_matrizes() {
    for (int i = 0; i < LINHA + 2; i++) {
        for (int j = 0; j < COLUNA + 2; j++) {
            int indice = (i * BORDA) + j;

            if (i == 0 || j == 0 || i == LINHA + 1 || j == COLUNA + 1) {
                estados_atuais[indice] = NAO_COMBUSTIVEL;
                estados_prox[indice] = NAO_COMBUSTIVEL;
                tempos_queima_atuais[indice] = TEMPO_NULO;
                tempos_queima_prox[indice] = TEMPO_NULO;
                fatores[indice] = FATOR_NULO;
                tempos_ativacao[indice] = -1;
                continue;
            }

            cobertura cb = avaliar_valor(rand_r(&SEED) % 100);
            umidades[indice] = rand_r(&SEED) % 101;
            tempos_ativacao[indice] = -1;
            tempos_queima_prox[indice] = TEMPO_NULO;

            switch (cb) {
                case VEGETACAO_RASTEIRA:
                    fatores[indice] = FATOR_VEGETACAO;
                    estados_atuais[indice] = INTACTA;
                    estados_prox[indice] = INTACTA;
                    tempos_queima_atuais[indice] = TEMPO_NULO;
                    celulas_combustiveis_inicial++;
                    break;

                case FLORESTA:
                    fatores[indice] = FATOR_FLORESTA;
                    estados_atuais[indice] = INTACTA;
                    estados_prox[indice] = INTACTA;
                    tempos_queima_atuais[indice] = TEMPO_NULO;
                    celulas_combustiveis_inicial++;
                    break;

                default:
                    fatores[indice] = FATOR_NULO;
                    estados_atuais[indice] = NAO_COMBUSTIVEL;
                    estados_prox[indice] = NAO_COMBUSTIVEL;
                    tempos_queima_atuais[indice] = TEMPO_NULO;
                    celulas_n_combustiveis++;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc > 2) {
        fprintf(stderr, "É necessário passar apenas o nome do arquivo\n");
        return 1;
    } else if (argc < 2) {
        fprintf(stderr, "É necessário passar o nome do arquivo\n");
        return 1;
    }

    FILE* fp;
    if (!aberturaArquivo(&fp, argv[1])) {
        fprintf(stderr, "Houve um problema na abertura do arquivo\n");
        return 1;
    }

    fscanf(fp, " %d %d %d %d %u %d", &LINHA, &COLUNA, &PASSOS, &THREADS, &SEED, &LIMIAR);
    if (!(verifica_gt_0(LINHA) && verifica_gt_0(COLUNA) && verifica_gte_0(PASSOS) && verifica_gt_0(THREADS) && verifica_gt_0(LIMIAR))) {
        fprintf(stderr, "Os parâmetros do header estão inválidos\n");
        return 1;
    }

    fscanf(fp, " %d %d %d", &VENTO_LINHA, &VENTO_COLUNA, &INTENSIDADE);
    if (!(verifica_vento(VENTO_LINHA) && verifica_vento(VENTO_COLUNA) && !(VENTO_COLUNA == 0 && VENTO_LINHA == 0) && verifica_intensidade(INTENSIDADE))) {
        fprintf(stderr, "Parâmetros de vento inválidos\n");
        return 1;
    }

    fscanf(fp, " %d %d", &N_FOCOS, &N_ZONAS);
    if (!(verifica_gte_0(N_FOCOS) && verifica_gte_0(N_ZONAS))) {
        fprintf(stderr, "O número de zonas e de focos deve ser maior ou igual a 0\n");
        return 1;
    }

    BORDA = COLUNA + 2;
    size_t total_celulas = (size_t)(LINHA + 2) * BORDA;

    estados_atuais = malloc(sizeof(estado) * total_celulas);
    estados_prox = malloc(sizeof(estado) * total_celulas);
    umidades = malloc(sizeof(int) * total_celulas);
    fatores = malloc(sizeof(fator_queima) * total_celulas);
    tempos_queima_atuais = malloc(sizeof(tempo_queima) * total_celulas);
    tempos_queima_prox = malloc(sizeof(tempo_queima) * total_celulas);
    tempos_ativacao = malloc(sizeof(int) * total_celulas);

    iniciar_matrizes();

    for (int i = 0; i < N_FOCOS; i++) {
        int linhaF, colunaF;
        fscanf(fp, " %d %d", &linhaF, &colunaF);

        if (!(verifica_linha(linhaF) && verifica_coluna(colunaF))) {
            fprintf(stderr, "Foco fora da matriz.\n");
            return 1;
        }

        int indice = ((linhaF + 1) * BORDA) + (colunaF + 1);

        if (estados_atuais[indice] == NAO_COMBUSTIVEL) {
            fprintf(stderr, "Foco em célula não combustível.\n");
            return 1;
        }
        if (estados_atuais[indice] == EM_CHAMAS) {
            fprintf(stderr, "Foco repetido.\n");
            return 1;
        }

        estados_atuais[indice] = EM_CHAMAS;
        estados_prox[indice] = EM_CHAMAS;
        tempos_queima_atuais[indice] = (fatores[indice] == FATOR_VEGETACAO) ? TEMPO_VEGETACAO : TEMPO_FLORESTA;
        tempos_queima_prox[indice] = tempos_queima_atuais[indice];
        celulas_em_chamas++;
    }

    for (int i = 0; i < N_ZONAS; i++) {
        int timestamp, linhaIZ, colunaIZ, linhaFZ, colunaFZ;
        fscanf(fp, " %d %d %d %d %d", &timestamp, &linhaIZ, &colunaIZ, &linhaFZ, &colunaFZ);

        if (!(verifica_linha(linhaIZ) && verifica_linha(linhaFZ) && verifica_coluna(colunaIZ) && verifica_coluna(colunaFZ))) {
            fprintf(stderr, "Zona fora da matriz.\n");
            return 1;
        }
        if ((linhaIZ > linhaFZ) || (colunaIZ > colunaFZ)) {
            fprintf(stderr, "Limites iniciais maiores do que os finais.\n");
            return 1;
        }
        if (timestamp < 0 || timestamp >= PASSOS) {
            fprintf(stderr, "Passo de ativação inválido.\n");
            return 1;
        }

        for (int m = linhaIZ; m <= linhaFZ; m++) {
            for (int n = colunaIZ; n <= colunaFZ; n++) {
                int indice = ((m + 1) * BORDA) + (n + 1);
                if (tempos_ativacao[indice] == -1 || timestamp < tempos_ativacao[indice]) {
                    tempos_ativacao[indice] = timestamp;
                }
            }
        }
    }
    fclose(fp);

    celulas_intactas = celulas_combustiveis_inicial - celulas_em_chamas;
    int passo_atual = 0;
    int ignicoes_no_passo = 0;
    int continuar = true;

    omp_set_dynamic(0);
    double tempo_inicial = omp_get_wtime();

    #pragma omp parallel num_threads(THREADS) default(none) \
        shared(estados_atuais, estados_prox, umidades, fatores, \
               tempos_queima_atuais, tempos_queima_prox, tempos_ativacao, \
               passo_atual, ignicoes_no_passo, continuar, \
               celulas_intactas, celulas_em_chamas, celulas_queimadas, celulas_de_contencao, \
               total_ignicoes, qnt_ignicoes_passo_maior, passo_com_maior_numero_de_ignicoes, \
               PASSOS, LINHA, COLUNA, BORDA, VENTO_LINHA, VENTO_COLUNA, INTENSIDADE, LIMIAR)
    {
        while (true) {
            #pragma omp single
            {
                continuar = (passo_atual < PASSOS && celulas_em_chamas > 0);
                ignicoes_no_passo = 0;
            }

            if (!continuar) break;

            // ativação das zonas
            #pragma omp for collapse(2) schedule(static) \
                reduction(+: celulas_intactas, celulas_de_contencao)
            for (int i = 0; i < LINHA; i++) {
                for (int j = 0; j < COLUNA; j++) {
                    int indice = ((i + 1) * BORDA) + (j + 1);
                    if (tempos_ativacao[indice] == passo_atual && estados_atuais[indice] == INTACTA) {
                        estados_prox[indice] = CONTENCAO;
                        celulas_de_contencao++;
                        celulas_intactas--;
                    }
                }
            }

            // cálculo do próximo estado
            #pragma omp for collapse(2) schedule(static) \
                reduction(+: celulas_intactas, celulas_em_chamas, celulas_queimadas, \
                             total_ignicoes, ignicoes_no_passo)
            for (int i = 0; i < LINHA; i++) {
                for (int j = 0; j < COLUNA; j++) {
                    int indice = ((i + 1) * BORDA) + (j + 1);

                    // célula já resolvida como CONTENCAO no laço anterior
                    if (tempos_ativacao[indice] == passo_atual && estados_atuais[indice] == INTACTA) {
                        continue;
                    }

                    estados_prox[indice] = estados_atuais[indice];
                    tempos_queima_prox[indice] = tempos_queima_atuais[indice];

                    if (estados_atuais[indice] == INTACTA) {
                        int peso_total = 0;
                        #pragma omp simd reduction(+: peso_total)
                        for (int k = 0; k < 8; k++) {
                            int i_vizinho = i + 1 + DIRS[k][0];
                            int j_vizinho = j + 1 + DIRS[k][1];
                            int indice_vizinho = (i_vizinho * BORDA) + j_vizinho;

                            int em_chamas = (estados_atuais[indice_vizinho] == EM_CHAMAS);

                            int alinhamento_vento = (VENTO_LINHA * (-DIRS[k][0])) + (VENTO_COLUNA * (-DIRS[k][1]));
                            int peso_vizinho = DIRS[k][2] + (INTENSIDADE * alinhamento_vento);
                            peso_total += (peso_vizinho > 1 ? peso_vizinho : 1) * em_chamas;
                        }

                        int potencial_ignicao = (peso_total * fatores[indice] * (100 - umidades[indice])) / 100;

                        if (potencial_ignicao >= LIMIAR) {
                            estados_prox[indice] = EM_CHAMAS;
                            tempos_queima_prox[indice] = (fatores[indice] == FATOR_VEGETACAO) ? TEMPO_VEGETACAO : TEMPO_FLORESTA;
                            celulas_em_chamas++;
                            celulas_intactas--;
                            total_ignicoes++;
                            ignicoes_no_passo++;
                        }
                    }
                    else if (estados_atuais[indice] == EM_CHAMAS) {
                        tempos_queima_prox[indice] = tempos_queima_atuais[indice] - 1;
                        if (tempos_queima_prox[indice] == 0) {
                            estados_prox[indice] = QUEIMADA;
                            celulas_em_chamas--;
                            celulas_queimadas++;
                        }
                    }
                }
            }

            #pragma omp single
            {
                if (ignicoes_no_passo > qnt_ignicoes_passo_maior) {
                    qnt_ignicoes_passo_maior = ignicoes_no_passo;
                    passo_com_maior_numero_de_ignicoes = passo_atual;
                }

                estado *aux_estados = estados_atuais;
                estados_atuais = estados_prox;
                estados_prox = aux_estados;

                tempo_queima *aux_tempos = tempos_queima_atuais;
                tempos_queima_atuais = tempos_queima_prox;
                tempos_queima_prox = aux_tempos;

                passo_atual++;
            }
        }
    }

    double tempo_final = omp_get_wtime();

    if (celulas_combustiveis_inicial == 0) {
        percentual_queimado = 0.0;
        percentual_protegido = 0.0;
    } else {
        percentual_queimado = 100 * ((celulas_queimadas + celulas_em_chamas) / (double)celulas_combustiveis_inicial);
        percentual_protegido = 100 * (celulas_de_contencao / (double)celulas_combustiveis_inicial);
    }

    unsigned long long checksum = 0;
    for (long long k = 0; k < (long long)LINHA * COLUNA; k++) {
        long long i = k / COLUNA;
        long long j = k % COLUNA;
        long long indice = ((i + 1) * BORDA) + (j + 1);

        checksum = checksum * 31ULL + (unsigned long long)estados_atuais[indice];
        checksum = checksum * 31ULL + (unsigned long long)tempos_queima_atuais[indice];
    }

    printf("passos: %d\n"
       "nao_combustiveis: %d\n"
       "intactas: %d\n"
       "em_chamas: %d\n"
       "queimadas: %d\n"
       "contencao: %d\n"
       "total_ignicoes: %d\n"
       "pico_ignicoes: %d %d\n"
       "percentual_queimado: %.2f\n"
       "percentual_protegido: %.2f\n"
       "checksum: %llu\n"
       "tempo: %f\n",
       passo_atual, celulas_n_combustiveis, celulas_intactas, celulas_em_chamas,
       celulas_queimadas, celulas_de_contencao, total_ignicoes,
       passo_com_maior_numero_de_ignicoes, qnt_ignicoes_passo_maior,
       percentual_queimado, percentual_protegido, checksum, tempo_final - tempo_inicial);

    free(estados_atuais);
    free(estados_prox);
    free(umidades);
    free(fatores);
    free(tempos_queima_atuais);
    free(tempos_queima_prox);
    free(tempos_ativacao);

    return 0;
}