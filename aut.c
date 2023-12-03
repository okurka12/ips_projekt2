/*****************
**  Vit Pavlik  **
**   xpavli0a   **
**    251301    **
*****************/

/**
 * Reseni druhe ulohy IPS 2023/2024
*/

/* toto kdyby tady nebylo tak si prekladac stezuje ze usleep neexistuje */
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <threads.h>  // nutne prelozit s flagem -lpthread (aspon u me)
#include <unistd.h>  // usleep
#include <string.h>  // strcmp


/*                          0  1         2    3     4    5       6    7       */
#define usage_msg "Pouziti: %s MIN_SCORE STR1 SC1 [ STR2 SC2 ] [ STR3 SC3 ] ..."

#define MAX_LINE_LENGTH 8192

/* pred odevzdanim oboji dat na 0 */
#define DEBUG_LOGS 1
#define DEBUG_USLEEP 1



#if DEBUG_LOGS
#define log(msg, ...) fprintf(stderr, msg, __VA_ARGS__); fflush(stderr)
#else  // if DEBUG_LOGS
#define log(msg, ...) {}
#endif  // if DEBUG_LOGS

#if DEBUG_USLEEP
#define uslp() usleep(1)
#else  // if DEBUG_USLEEP
#define uslp() {}
#endif  // if DEBUG_USLEEP

/* -------------------------- globalni promenne ----------------------------- */
mtx_t finished_mtx;
bool finished;

/* zamek k `score` */
mtx_t score_mtx;

int score;

/* pole pro zamky signalizujici, ze vlakno je hotove */
mtx_t *done_mtx_arr;

/* argv ktere dostal main */
char **argv_global;

/**
 * aktualni radek
 * @note nema zamek protoze vlakna z nej budou jenom cist
*/
char *line;

char *read_line() {
    char *line = (char *)malloc(sizeof(char) * MAX_LINE_LENGTH);
    if (line == NULL) return NULL;

    fgets(line, MAX_LINE_LENGTH, stdin);

    for (unsigned int i = 0; i < MAX_LINE_LENGTH; i++) {
        if (line[i] == '\n') {
            line[i] = '\0';
            break;
        }
    }

    return line;
}

/**
 * Alokuje pole mutexu o velikosti `size`. Zamky v poli budou inicializovane
 * a zamcene. Pri neuspechu malloc vrati NULL
 */
mtx_t *alloc_mtx_arr(unsigned int size) {

    /* kontrola at se nestrelim do nohy */
    if (size == 0) {
        fprintf(stderr, "Jsi si jisty ze chces alokovat pole 0 zamku?\n");
    }

    /* alokace */
    mtx_t *output = (mtx_t *)malloc(size * sizeof(mtx_t));
    if (output == NULL) return NULL;

    for (unsigned int i = 0; i < size; i++) {
        mtx_init(output + i, mtx_plain);
        mtx_lock(output + i);
    }

    return output;
}

/**
 * Alokuje pole intu o velikosti `size`, prvky inicializovany na 0
*/
int *alloc_int_arr(unsigned int size) {

    /* alokace */
    int *output = (int *)malloc(size * sizeof(int));
    if (output == NULL) return NULL;

    /* inicializace */
    for (unsigned int i = 0; i < size; i++) {
        output[i] = 0;
    }

    return output;
}

/**
 * alokuje pole unsigned intu a jejich hodnoty nastavi tak, aby hodnota prvku
 * odpovidala jeho indexu (pole bude mit velikost `size`)
*/
unsigned int *alloc_unsigned_arr(unsigned int size) {
    /* alokace */
    unsigned int *output = (unsigned int *)malloc(size * sizeof(unsigned int));
    if (output == NULL) return NULL;

    /* inicializace */
    for (unsigned int i = 0; i < size; i++) {
        output[i] = i;
    }

    return output;

}

/**
 * Alokuje pole boolu o velikosti `size`, prvky inicializovany na false
*/
bool *alloc_bool_arr(unsigned int size) {

    /* alokace */
    bool *output = (bool *)malloc(size * sizeof(bool));
    if (output == NULL) return NULL;

    /* inicializace */
    for (unsigned int i = 0; i < size; i++) {
        output[i] = false;
    }

    return output;
}

/**
 * Alokuje pole identifikatoru vlaken thrd_t
*/
thrd_t *alloc_thrd_arr(unsigned int size) {

    /* alokace */
    thrd_t *output = (thrd_t *)malloc(size * sizeof(thrd_t));
    return output;
}

bool is_even(int n) {
    return (n % 2 == 0);
}

/* vrati ukazatel na str-i (cislovano od nuly) */
char *get_str(char **argv, unsigned int i) {
    unsigned int idx = (i * 2) + 2;
    return argv[idx];
}

/* vrati ukazatel na sc-i (cislovano od nuly) */
char *get_sc(char **argv, unsigned int i) {
    unsigned int idx = (i * 2) + 3;
    return argv[idx];
}

/* automat ktery se spusti ve vlaknu */
int fsm(void *arg) {

    /* zjistit kdo jsem */
    uslp();
    unsigned int id = * ((unsigned int *)arg);
    uslp();
    log("vlakno %u se spustilo\n", id);

    /* precteni scr-i */
    int scr;
    int scanned = sscanf(get_sc(argv_global, id), "%d", &scr);
    if (scanned != 1) {
        fprintf(stderr, "%s neni platne cislo\n", get_sc(argv_global, id));
        goto return_fail;
    }

    /* nastaveni ukazovatka na str-i */
    char *str = get_str(argv_global, id);

    while (true) {

        /* podivat se jestli jsme neskoncili uz */
        mtx_lock(&finished_mtx);
        if (finished) {
            log("%s", "finished=true\n");
            mtx_unlock(&finished_mtx);
            goto return_ok;
        }
        mtx_unlock(&finished_mtx);

        /* zacit pracovat */
        mtx_lock(done_mtx_arr + id);
        log("vlakno %u pracuje\n", id);

        /* naivni pristup k hledani podretezce */
        for (unsigned int i = 0; line[i] != '\0'; i++) {
            if (strcmp(line + i, str) == 0) {
                mtx_lock(&score_mtx);
                score += scr;
                mtx_unlock(&score_mtx);
            }
        }

        /* skoncit s praci */
        mtx_unlock(done_mtx_arr + id);
        uslp();
    }

    return_ok:
    log("vlakno %u uspesne skoncilo\n", id);
    return 0;

    return_fail:
    log("vlakno %u skoncilo NEUSPESNE\n", id);
    return 1;
}

/**
 * zamce vsechny zamky pro jednotliva vlakna - pocka dokud vsechna vlakna
 * nedokonci praci 
 */
void lock_all(unsigned int thr_cnt) {
    for (unsigned int i = 0; i < thr_cnt; i++) {
        mtx_lock(done_mtx_arr + i);
    }
}

/**
 * odemce vsechny zamky pro jednotliva vlakna - umozni jim pracovat
 */
void unlock_all(unsigned int thr_cnt) {
    for (unsigned int i = 0; i < thr_cnt; i++) {
        mtx_unlock(done_mtx_arr + i);
    }
}

/* makro pro `main()` - zkontroluje `p` jestli neni NULL a kdyztak vrati 1 */
#define check(p) if ((p) == NULL) do {\
    fprintf(stderr, "Selhal malloc " #p "\n"); \
    return 1; \
} while (0)

int main(int argc, char **argv) {

    argv_global = argv;

    /* kontrola poctu argumentu */
    if (!is_even(argc)) {
        fprintf(stderr, usage_msg, argv[0]);
        return 1;
    }

    /* precteni min_scr */
    int min_scr;
    int scanned = sscanf(argv[1], "%d", &min_scr);
    if (scanned != 1) {
        fprintf(stderr, "%s neni platne minimalni skore\n", argv[1]);
    }

    mtx_init(&score_mtx, mtx_plain);

    /* inicializace globalni promenne `finished` */
    mtx_init(&finished_mtx, mtx_plain);
    mtx_init(&finished_mtx, mtx_plain);
    mtx_lock(&finished_mtx);
    finished = false;
    mtx_unlock(&finished_mtx);

    /* pocet vlaken */
    unsigned int thr_cnt = (argc - 2) / 2;

    /* alokace poli */
    done_mtx_arr = alloc_mtx_arr(thr_cnt);
    check(done_mtx_arr);
    thrd_t *thrd_ids = alloc_thrd_arr(thr_cnt);
    check(thrd_ids);

       
    /**
     * id pro predani vlaknum jako parametr
     * 
     * @note proc je tohle nutne? protoze promanna `i` ve for cyklu se muze zmenit 
     * nez si ji precte vytvorene vlakno
     */
    unsigned int *ids = alloc_unsigned_arr(thr_cnt);
    check(ids);

    /* spusteni vlaken */
    for (unsigned int i = 0; i < thr_cnt; i++) {
        mtx_init(done_mtx_arr + i, mtx_plain);
        mtx_lock(done_mtx_arr + i);
        thrd_create(thrd_ids + i, fsm, ids + i);
        uslp();
    }

    while (!feof(stdin)) {
        line = read_line();
        log("main: radek '%s'\n", line);
        unlock_all(thr_cnt);
        lock_all(thr_cnt);
        log("main: vlakna neaktivni\n%s", "");
        if (score >= min_scr) {
            puts(line);
        }
    }
    unlock_all(thr_cnt);

    /* skoncit */
    log("main: snazim se skoncit%s\n", "");
    mtx_lock(&finished_mtx);
    finished = true;
    mtx_unlock(&finished_mtx);

    /* pockani na vlakna */
    int rcode;
    log("main: cekam na ukonceni vlaken%s\n", "");
    for (unsigned int i = 0; i < thr_cnt; i++) {
        thrd_join(thrd_ids[i], &rcode);

        /* pokud vlakno skoncilo chybou tak se na ostatni ani neceka */
        if (rcode != 0) return rcode;
    }


    /* uklid */
    free(done_mtx_arr);
    free(thrd_ids);
    free(ids);

}