# CustomHtop

CustomHtop - небольшой веб-интерфейс в стиле htop. Сервер на C++ читает данные из `/proc`, отдает JSON API и статические файлы из каталога `web/`.

## Требования

- Linux с доступным `/proc`.
- `g++` с поддержкой C++17.
- `make`.
- POSIX threads, подключаются флагом `-pthread` из `Makefile`.

## Сборка

```bash
make
```

Команда собирает бинарный файл `custom_htop` из `main.cpp` и всех файлов `src/*.cpp`:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic -pthread -Iinclude -o custom_htop main.cpp src/*.cpp
```

Если нужно переопределить компилятор или флаги:

```bash
make CXX=clang++ CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -pedantic -pthread"
```

## Запуск

Через цель `run`:

```bash
make run
```

По умолчанию приложение слушает `http://127.0.0.1:8080`.

Можно указать порт первым аргументом:

```bash
./custom_htop 9090
```

Или через переменную окружения:

```bash
PORT=9090 ./custom_htop
```

Аргумент командной строки имеет приоритет над `PORT`.

## Очистка

```bash
make clean
```

Удаляет собранный бинарный файл `custom_htop`.

## Структура проекта

- `main.cpp` - точка входа и выбор порта.
- `include/` - объявления классов, структур и утилит.
- `src/` - реализация сервера, сборщика процессов и вспомогательных функций.
- `web/` - статический HTML/CSS/JS интерфейс.

## API

- `GET /api/snapshot` - снимок CPU, памяти и процессов.
- `GET /api/snapshot?threads=1` - снимок с отдельными строками потоков.
- `GET /api/signals` - список доступных сигналов.
- `POST /api/signal` - отправка сигнала процессу, тело: `{"pid":123,"signal":15}`.
