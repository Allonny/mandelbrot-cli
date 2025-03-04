#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <unistd.h>
#include <inttypes.h>

#define DELAY 10000

// Структура аргументов коммандной строки
typedef struct option
{
	char arg1[3];
	char arg2[32];
	char info[256];
} option;

// Структура комплексного числа
typedef struct complex
{
	long double r;
	long double i;
	int8_t flag;
} complex;


// Структура поля визуализации
typedef struct field
{
	complex **point;
	size_t rows, cols;
} field;

// Структура диапозона визуализации
typedef struct range
{
	long double x1, x2, y1, y2;
} range;

// Коды завершения программы
enum error {
	NORMAL,
	UNKNOWN_ARG,
	MISSING_PARAM,
	INCORRECT_RANGE,
	INCORRECT_FILE
};

// Аргументы коммандной строки
const option options[] = {
	{"-r", "--range", "Visualisated part of Mandelbrot's set. Format: x:<Real number>..<Real number>/y:<Real number>..<Real number> or c:<Real number>+<Real number>i/w:<Real number>"},
	{"-f", "--file", "Using range from file. Format: <path/to/file>"},
	{"-h", "--help", "Print current help page."},
	{"", "", ""}
};

// Заголовок страницы справки
const char help_promt[] = "This is simple Mandelbrot's set visualisation program.";

// Глобальные переменные: структура параметров окна терминала, структура поля, структура диапозона
struct winsize ws;
struct field fl = {.point = NULL, .rows = 0, .cols = 0};
struct range rg;
struct range rg_def = {.x1 = -2.5, .x2 = 1., .y1 = 0, .y2 = 0};
int8_t enable_step = 1;

void echo_disable(int8_t flag)
{
	struct termios setup;
	tcgetattr(fileno(stdin), &setup);
	if (flag) setup.c_lflag &= ~ECHO;
	else setup.c_lflag |= ECHO;
	tcsetattr(fileno(stdin), 0, &setup);
}

// Выход из программы
void final (enum error code, char *msg)
{
	switch (code)
	{
		case UNKNOWN_ARG:
			printf("Unknown argument %s\n", msg);
			break;
		case MISSING_PARAM:
			printf("Missing parameter to %s argument\n", msg);
			break;
		case INCORRECT_RANGE:
			printf("Incorrect range: %s\n", msg);
			break;
		case INCORRECT_FILE:
			printf("Incorrect file name: %s\n", msg);
			break;
		default:
			break;
	}

	echo_disable(0);
	exit(code);
}

// Комплексное сложение
complex c_add (complex a, complex b)
{
	complex c = {.r = a.r + b.r, .i = a.i + b.i};
	return c;
}

// Комплексное умножение
complex c_mul (complex a, complex b)
{
	complex c = {.r = a.r * b.r - a.i * b.i, .i = a.r * b.i + a.i * b.r};
	return c;
}

// Модуль комплексного числа
long double c_abs (complex a)
{
	long double l = a.r * a.r + a.i * a.i;
	return l;
}

// Получение размеров окна терминала (с учётом зарезервированной строки для информации о диапозоне)
void get_winsize (struct winsize *ws)
{
	ioctl(0, TIOCGWINSZ, ws);
	ws->ws_row -= 1;
}

// Вывод поля визуализации
void field_out ()
{
	printf("\e[0;0H");

	for (size_t i = 0; i < fl.rows; i += 4)
	{
		for (size_t j = 0; j < fl.cols; j += 2)
		{
			// Формирование символа шрифта Брайля
			char pattern[4] = {0xE2, 0xA0, 0x80, 0};
			pattern[2] +=
				(fl.point[i][j].flag << 0) +
				(fl.point[i+1][j].flag << 1) +
				(fl.point[i+2][j].flag << 2) +
				(fl.point[i][j+1].flag << 3) +
				(fl.point[i+1][j+1].flag << 4) +
				(fl.point[i+2][j+1].flag << 5);
			pattern[1] +=
				(fl.point[i+3][j].flag << 0) +
				(fl.point[i+3][j+1].flag << 1);

			printf("%s", pattern);
		}
		printf("\n");
	}
	
	long double x0 = (rg.x1 + rg.x2) * 0.5;
	long double y0 = (rg.y1 + rg.y2) * 0.5;

	long double dx = (rg.x2 - rg.x1) * 0.5;
	long double dy = (rg.y2 - rg.y1) * 0.5;

	printf("Center: %Lf ± %Le, %Lf ± %Le\e[0;0H", x0, dx, y0, dy);
}

// Очистка поля по SIGINT
void field_clean (int signum)
{
	printf("\e[2J\e[0;0H");
	printf("~Made by Allonny~\n");
	final(NORMAL, NULL);
}

// Инициализация поля
void field_init ()
{
	for (size_t i = 0; i < fl.rows; i++)
		free(fl.point[i]);
	free(fl.point);

	get_winsize(&ws);
	fl.rows = ws.ws_row << 2;
	fl.cols = ws.ws_col << 1;
	fl.point = calloc(fl.rows, sizeof(*fl.point));
	for (size_t i = 0; i < fl.rows; i++)
		fl.point[i] = calloc(fl.cols, sizeof(**fl.point));
}

// Заполенение поля
void field_fill ()
{
	rg = rg_def;

	long double dx = (rg.x2 - rg.x1) / fl.cols;
	if (rg.y1 - rg.y2 == 0)
	{
		long double ratio = 0.5 * fl.rows / fl.cols * (rg.x2 - rg.x1);
		rg.y1 = rg.y1 - ratio;
		rg.y2 = rg.y2 + ratio;
	}
	long double dy = (rg.y1 - rg.y2) / fl.rows;

	for (size_t i = 0; i < fl.rows; i++)
		for (size_t j = 0; j < fl.cols; j++)
			fl.point[i][j] = (complex){.r = rg.x1 + (j + 0.5) * dx, .i = rg.y2 + (i + 0.5) * dy, .flag = 1};
}

// Шаг визуализации
void field_step ()
{
	long double dx = (rg.x2 - rg.x1) / fl.cols;
	long double dy = (rg.y1 - rg.y2) / fl.rows;

	for (size_t i = 0; i < fl.rows; i++)
		for (size_t j = 0; j < fl.cols; j++)
		{
			if (!fl.point[i][j].flag) continue;
			complex c = {.r = rg.x1 + (j + 0.5) * dx, .i = rg.y2 + (i + 0.5) * dy};
			fl.point[i][j] = c_add(c_mul(fl.point[i][j], fl.point[i][j]), c);
			fl.point[i][j].flag = c_abs(fl.point[i][j]) <= 4.;
		}
}

// Изменение размеров окна
void resize (int signum)
{
	field_init();
	field_fill();
}

void enable_toggle (int signum)
{
	enable_step = enable_step ? 0 : 1;
}

// Парсинг текстового представления диапозона визуализации в формате:
// x:<Действительное число>..<Действительное число>/y:<Действительное число>..<Действительное число>
// c:<Действительное число>+<Действительное число>i/w:<Действительное число>
int range_parse (char *arg)
{
	long double x1, x2, y1, y2, w;

	if(sscanf(arg, "x:%Lf..%Lf/y:%Lf..%Lf", &x1, &x2, &y1, &y2) == 4)
	{
		rg_def.x1 = x1;
		rg_def.x2 = x2;
		rg_def.y1 = y1;
		rg_def.y2 = y2;
		return 0;
	}
	if(sscanf(arg, "c:%Lf+%Lfi/w:%Lf", &x1, &y1, &w) == 3)
	{
		rg_def.x1 = x1 - w * 0.5;
		rg_def.x2 = x1 + w * 0.5;
		rg_def.y1 = y1;
		rg_def.y2 = y1;
		return 0;
	}

	return 1;
}

// Считывание текстового файла для задания координат визуализацииыы
int file_parse (char *arg)
{
	FILE *f;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	f = fopen(arg, "r");
	if (!f) return 1;
	
	read = getline(&line, &len, f);
	fclose(f);

	if (read == -1) return 1;
	if (range_parse(line)) final(INCORRECT_RANGE, line);

	if(line) free(line);

	return 0;
}

// Вывод справки
int help_out ()
{
	printf("%s\n", help_promt);
	for (size_t i = 0; strlen(options[i].arg1); i++)
	{
		printf("\n  %s  %s\n", options[i].arg1, options[i].arg2);
		printf("\t%s\n", options[i].info);
	}

	return 0;
}

// Парсинг аргументов коммандной строки
void args_parse (int argc, char *argv[])
{
	for	(size_t i = 1; i < argc; i++)
	{
		if (!(strcmp(argv[i], options[0].arg1) && strcmp(argv[i], options[0].arg2)))
		{
			if (++i >= argc) final(MISSING_PARAM, argv[i-1]);
			if (range_parse(argv[i])) final(INCORRECT_RANGE, argv[i]);
			continue;
		}
		if (!(strcmp(argv[i], options[1].arg1) && strcmp(argv[i], options[1].arg2)))
		{
			if (++i >= argc) final(MISSING_PARAM, argv[i-1]);
			if (file_parse(argv[i])) final(INCORRECT_FILE, argv[i]);
			continue;
		}
		if (!(strcmp(argv[i], options[2].arg1) && strcmp(argv[i], options[2].arg2)))
		{
			help_out();
			final(NORMAL, NULL);
		}
		final(UNKNOWN_ARG, argv[i]);
	}
}

// Главный вход
int main (int argc, char *argv[])
{
	// Парсинг аргументов коммандной строки
	if (argc > 1)
		args_parse(argc, argv);

	// Объявление слушаетлей сигналов
	signal(SIGINT, field_clean);
	signal(SIGWINCH, resize);
	signal(SIGTSTP, enable_toggle);

	echo_disable(1);

	// Начальные инициализация и заполнение поля
	field_init();
	field_fill();
	field_out();
	
	// Цикл визуализации
	while (1)
	{
		usleep(DELAY);
		if (enable_step) field_step();
		field_out();
	}

	field_clean(0);
	return 0;
}
