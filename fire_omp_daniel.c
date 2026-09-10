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

/* definição dos arrays para os atributos */
// diferente da abordagem do fernando de separar a matriz como um todo em células (que, aliás, pra mim 
// é mais intuitiva), essa abordagem usa os atributos das células como arrays separados e deve ter uma 
// performance melhor se tratando de cache (menos cache misses), uma vez que os atributos estão em arrays 
// separados (e não em structs separadas)
estado *estados_atuais;
estado *estados_prox;
int *umidades;
fator_queima *fatores;
tempo_queima *tempos_queima_atuais;
tempo_queima *tempos_queima_prox;
int *tempos_ativacao;

//    L   |   C   |   P   |   T   |  S  | LIMIAR
int LINHA, COLUNA, PASSOS, THREADS, SEED, LIMIAR;
int VENTO_LINHA, VENTO_COLUNA, INTENSIDADE;
int N_FOCOS, N_ZONAS;

int celulas_n_combustiveis = 0, celulas_combustiveis_inicial = 0;
int celulas_intactas = 0, celulas_em_chamas = 0, celulas_queimadas = 0, celulas_de_contencao = 0;

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

void iniciar_matrizes() {
    for(int i = 0; i < LINHA; i++) {
        for(int j = 0; j < COLUNA; j++) {
            int indice = (i * COLUNA) + j;
            cobertura cb = avaliar_valor(rand_r(&SEED) % 100);
            umidades[indice] = rand_r(&SEED) % 101;
            tempos_ativacao[indice] = -1;

            switch (cb) {
                case VEGETACAO_RASTEIRA:
                    fatores[indice] = FATOR_VEGETACAO;
                    estados_atuais[indice] = INTACTA;
                    tempos_queima_atuais[indice] = TEMPO_VEGETACAO;
                    celulas_combustiveis_inicial++;
                    break;

                case FLORESTA:
                    fatores[indice] = FATOR_FLORESTA;
                    estados_atuais[indice] = INTACTA;
                    tempos_queima_atuais[indice] = TEMPO_FLORESTA;
                    celulas_combustiveis_inicial++;
                    break;

                default:
                    fatores[indice] = FATOR_NULO;
                    estados_atuais[indice] = NAO_COMBUSTIVEL;
                    tempos_queima_atuais[indice] = TEMPO_NULO;
                    celulas_n_combustiveis++;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "É necessário passar apenas o nome do arquivo\n");
        return 1;
    }

    FILE* fp;
    if (!aberturaArquivo(&fp, argv[1])){
        fprintf(stderr, "Houve um problema na abertura do arquivo\n");
        return 1;
    }

    fscanf(fp, " %d %d %d %d %d %d", &LINHA, &COLUNA, &PASSOS, &THREADS, &SEED, &LIMIAR);
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
    if(!(verifica_gte_0(N_FOCOS) && verifica_gte_0(N_ZONAS))) {
        fprintf(stderr, "O número de zonas e de focos deve ser maior ou igual a 0\n");
        return 1;
    }

    estados_atuais = malloc(sizeof(estado) * LINHA * COLUNA);
    estados_prox = malloc(sizeof(estado) * LINHA * COLUNA);
    umidades = malloc(sizeof(int) * LINHA * COLUNA);
    fatores = malloc(sizeof(fator_queima) * LINHA * COLUNA);
    tempos_queima_atuais = malloc(sizeof(tempo_queima) * LINHA * COLUNA);
    tempos_queima_prox = malloc(sizeof(tempo_queima) * LINHA * COLUNA);
    tempos_ativacao = malloc(sizeof(int) * LINHA * COLUNA);

    iniciar_matrizes();

    for (int i = 0; i < N_FOCOS; i++) {
        int linhaF, colunaF;
        fscanf(fp, " %d %d", &linhaF, &colunaF);
        
        if (!(verifica_linha(linhaF) && verifica_coluna(colunaF))) {
            fprintf(stderr, "Foco fora da matriz.\n");
            return 1;
        }
        
        int indice = (linhaF * COLUNA) + colunaF;
        
        if (estados_atuais[indice] == NAO_COMBUSTIVEL) {
            fprintf(stderr, "Foco em célula não combustível.\n");
            return 1;
        }
        if (estados_atuais[indice] == EM_CHAMAS) {
            fprintf(stderr, "Foco repetido.\n");
            return 1;
        }
        
        estados_atuais[indice] = EM_CHAMAS;
        celulas_em_chamas++;
    }
    
    for(int i = 0; i < N_ZONAS; i++) {
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
        
        for(int m = linhaIZ; m <= linhaFZ; m++) {
            for(int n = colunaIZ; n <= colunaFZ; n++) {
                int indice = (m * COLUNA) + n;
                if (tempos_ativacao[indice] == -1 || timestamp < tempos_ativacao[indice]) {
                    tempos_ativacao[indice] = timestamp;
                }
            }
        }
    }
    fclose(fp);

    for(int i = 0; i < LINHA; i++) {
        for(int j = 0; j < COLUNA; j++) {
            int indice = (i * COLUNA) + j;
            estados_prox[indice] = estados_atuais[indice];
            tempos_queima_prox[indice] = tempos_queima_atuais[indice];
        }
    }

    celulas_intactas = celulas_combustiveis_inicial - celulas_em_chamas;

    // aqui ficaria a lógica de simulação do incêndio usando o OpenMP de fato
    // TODO: fazer depois pq agora cansei ja #ehtoiss

    free(estados_atuais);
    free(estados_prox);
    free(umidades);
    free(fatores);
    free(tempos_queima_atuais);
    free(tempos_queima_prox);
    free(tempos_ativacao);
    
    return 0;
}