# Не просто создаём образ, но даём ему имя build
FROM gcc:12 as build

RUN apt update && \
    apt install -y python3-pip cmake && \
    pip3 install --break-system-packages conan==1.*

# Запуск conan как раньше
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11

# Папка data больше не нужна
COPY ./src /app/src
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF .. && \
    cmake --build .

# Второй контейнер в том же докерфайле
FROM ubuntu:22.04 as run

# Создадим пользователя www
RUN groupadd -r www && useradd -r -g www www


# Скопируем приложение со сборочного контейнера в директорию /app.
# Не забываем также папку data, она пригодится.
COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data
COPY ./tests /app/tests
COPY ./static /app/static

WORKDIR /app
EXPOSE 8080

# Запускаем игровой сервер
CMD ["/app/game_server", "--config-file", "/app/data/config.json", "--www-root", "/app/static"]

