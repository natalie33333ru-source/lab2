#ifndef __APPLE__                  /* на macOS этот макрос прячет clock_gettime */
#define _POSIX_C_SOURCE 199309L
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_ADDENDS  7
#define MAX_WORDS    (MAX_ADDENDS + 1)
#define MAX_WORD_LEN 18            /* чтобы число поместилось в long long */
#define MAX_LETTERS  10

// 1. РАЗБОР РЕБУСА, ВЫВОД И ПРОВЕРКА ОТВЕТА
typedef struct {
    int  n_addends;                          /* количество слагаемых */
    char words[MAX_WORDS][MAX_WORD_LEN + 1]; /* words[n_addends] — результат */
    int  len[MAX_WORDS];                     /* длины слов */
    int  n_letters;                          /* количество разных букв */
    char letters[MAX_LETTERS];               /* разные буквы в порядке появления */
} puzzle_t;


static int puzzle_parse(const char *text, puzzle_t *p)
{
    memset(p, 0, sizeof(*p));
    int seen[26] = {0};
    int word = 0, in_word = 0, got_eq = 0;

    for (const char *c = text;; ++c) {
        if (isupper((unsigned char)*c)) {
            if (word >= MAX_WORDS || p->len[word] >= MAX_WORD_LEN)
                return -1;
            p->words[word][p->len[word]++] = *c;
            in_word = 1;
            int k = *c - 'A';
            if (!seen[k]) {
                if (p->n_letters >= MAX_LETTERS)
                    return -1;
                seen[k] = 1;
                p->letters[p->n_letters++] = *c;
            }
            continue;
        }
        if (in_word) {
            ++word;
            in_word = 0;
        }
        if (*c == '\0')
            break;
        if (*c == '+') {
            if (got_eq) return -1;
        } else if (*c == '=') {
            if (got_eq) return -1;
            got_eq = 1;
            p->n_addends = word;
        } else if (!isspace((unsigned char)*c)) {
            return -1;                           /* недопустимый символ */
        }
    }
    if (!got_eq || word != p->n_addends + 1)
        return -1;
    if (p->n_addends < 2 || p->n_addends > MAX_ADDENDS)
        return -1;
    return 0;
}

/* Строит строку-ответ по отображению буква -> цифра (digit_of['A'..'Z']). */
static void puzzle_format(const puzzle_t *p, const int digit_of[26], char *out, size_t outsz)
{
    size_t pos = 0;
    out[0] = '\0';
    for (int w = 0; w <= p->n_addends; ++w) {
        if (w > 0 && pos < outsz)
            pos += snprintf(out + pos, outsz - pos, w == p->n_addends ? " = " : " + ");
        for (int i = 0; i < p->len[w] && pos + 1 < outsz; ++i)
            out[pos++] = (char)('0' + digit_of[p->words[w][i] - 'A']);
        if (pos < outsz)
            out[pos] = '\0';
    }
}

/* Проверяет, что answer — верное решение ребуса text. 1 — да, 0 — нет.*/
static int puzzle_verify(const char *text, const char *answer)
{
    puzzle_t p;
    if (puzzle_parse(text, &p) != 0)
        return 0;

    int digit_of[26], letter_of[10];
    for (int i = 0; i < 26; ++i) digit_of[i] = -1;
    for (int i = 0; i < 10; ++i) letter_of[i] = -1;

    const char *a = answer;
    long long sum = 0, result = 0;
    for (int w = 0; w <= p.n_addends; ++w) {
        while (*a && !isdigit((unsigned char)*a)) ++a;      /* к началу числа */
        if (p.len[w] > 1 && *a == '0') return 0;            /* ведущий ноль */
        long long v = 0;
        for (int i = 0; i < p.len[w]; ++i, ++a) {
            if (!isdigit((unsigned char)*a)) return 0;      /* число короче слова */
            int L = p.words[w][i] - 'A', d = *a - '0';
            if (digit_of[L] == -1 && letter_of[d] == -1) {
                digit_of[L] = d;
                letter_of[d] = L;
            } else if (digit_of[L] != d || letter_of[d] != L) {
                return 0;                                    /* нарушена биекция */
            }
            v = v * 10 + d;
        }
        if (isdigit((unsigned char)*a)) return 0;           /* число длиннее слова */
        if (w < p.n_addends) sum += v; else result = v;
    }
    return sum == result;
}

//2. РЕШАТЕЛЬ
typedef struct {
    const puzzle_t *p;
    int digit_of[26];   /* цифра каждой буквы */
    int used[10];       /* занята ли цифра */
} state_t;

static long long word_value(const state_t *s, int w)
{
    long long v = 0;
    for (int i = 0; i < s->p->len[w]; ++i)
        v = v * 10 + s->digit_of[s->p->words[w][i] - 'A'];
    return v;
}

static int check(const state_t *s)
{
    const puzzle_t *p = s->p;
    for (int w = 0; w <= p->n_addends; ++w)
        if (p->len[w] > 1 && s->digit_of[p->words[w][0] - 'A'] == 0)
            return 0;
    long long sum = 0;
    for (int w = 0; w < p->n_addends; ++w)
        sum += word_value(s, w);
    return sum == word_value(s, p->n_addends);
}

static int search(state_t *s, int k)
{
    if (k == s->p->n_letters)
        return check(s);
    int L = s->p->letters[k] - 'A';
    for (int d = 0; d < 10; ++d) {
        if (s->used[d])
            continue;
        s->used[d] = 1;
        s->digit_of[L] = d;
        if (search(s, k + 1))
            return 1;
        s->used[d] = 0;
    }
    return 0;
}

int solve(const char *text, char *out, size_t outsz)
{
    puzzle_t p;
    if (puzzle_parse(text, &p) != 0)
        return -1;
    state_t s = {.p = &p};
    if (!search(&s, 0))
        return 0;
    puzzle_format(&p, s.digit_of, out, outsz);
    return 1;
}

/* 3. ЗАМЕР ВРЕМЕНИ
 * Каждый ребус из файла решается, ответ проверяется puzzle_verify().
 * Быстрые решения повторяются сериями >= 2 мс (иначе не хватает
 * точности таймера); серия запускается reps раз, берётся минимум.*/

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

static int bench(const char *path, int reps)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return 2; }

    char line[512], out[256];
    double total = 0;
    int n = 0, bad = 0;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0] || line[0] == '#') continue;

        int res = solve(line, out, sizeof out);
        int ok = (res == 1) && puzzle_verify(line, out);

        long inner = 1;
        for (;;) {
            double t0 = now_ms();
            for (long i = 0; i < inner; ++i) solve(line, out, sizeof out);
            if (now_ms() - t0 >= 2.0 || inner >= (1L << 24)) break;
            inner *= 2;
        }
        double best = 1e18;
        for (int r = 0; r < reps; ++r) {
            double t0 = now_ms();
            for (long i = 0; i < inner; ++i) solve(line, out, sizeof out);
            double dt = (now_ms() - t0) / inner;
            if (dt < best) best = dt;
        }
        if (!ok) { ++bad; snprintf(out, sizeof out, "ОШИБКА"); }
        total += best;
        ++n;
        printf("%-50s %-45s %12.6f ms\n", line, out, best);
    }
    fclose(f);
    printf("\nребусов: %d, ошибок: %d, суммарное время: %.6f ms\n", n, bad, total);
    return bad ? 1 : 0;
}

//4. MAIN


static int read_line(const char *prompt, char *buf, size_t size)
{
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, (int)size, stdin))
        return -1;
    buf[strcspn(buf, "\r\n")] = '\0';
    return 0;
}

static void solve_and_print(const char *text)
{
    char out[256];
    int r = solve(text, out, sizeof out);
    if (r == 1)      printf("Решение: %s\n", out);
    else if (r == 0) printf("Решений нет\n");
    else             printf("Ошибка формата: %s\n", text);
}

static const char *find_puzzles_file(void)
{
    static const char *candidates[] = {"puzzles.txt", "../puzzles.txt"};
    for (size_t i = 0; i < sizeof candidates / sizeof candidates[0]; ++i) {
        FILE *f = fopen(candidates[i], "r");
        if (f) { fclose(f); return candidates[i]; }
    }
    return NULL;
}

static void menu(void)
{
    char line[512];
    for (;;) {
        printf("\n1 — решить ребус\n"
               "2 — замер времени по набору ребусов (puzzles.txt)\n"
               "0 — выход\n");
        if (read_line("Выбор: ", line, sizeof line) != 0 || line[0] == '0')
            return;
        if (line[0] == '1') {
            if (read_line("Введите ребус (например SEND + MORE = MONEY): ",
                          line, sizeof line) != 0)
                return;
            solve_and_print(line);
        } else if (line[0] == '2') {
            const char *path = find_puzzles_file();
            if (path)
                bench(path, 5);
            else
                printf("Файл puzzles.txt не найден: положите его рядом с cryptarithm.c\n");
        } else {
            printf("Нет такого пункта\n");
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        menu();
        return 0;
    }
    if (strcmp(argv[1], "--bench") == 0)
        return bench(argc >= 3 ? argv[2] : "puzzles.txt", argc >= 4 ? atoi(argv[3]) : 5);

    char text[512] = "";
    for (int i = 1; i < argc; ++i) {
        strncat(text, argv[i], sizeof text - strlen(text) - 2);
        strcat(text, " ");
    }
    solve_and_print(text);
    return 0;
}
