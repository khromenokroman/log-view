# LogViewer

![LogView preview](logo/logo.png)

**LogView** — веб-сервис для удобного просмотра логов в браузере.

Приложение позволяет загрузить файл с JSON-логами, сохранить его на сервере и получить уникальную ссылку для повторного просмотра. Этой ссылкой можно поделиться с другим человеком, чтобы он тоже мог открыть тот же лог без повторной загрузки файла.

Сервис отображает записи в удобном и читаемом виде: с цветовой подсветкой по уровню важности, форматированием времени, имени хоста, идентификатора процесса и текста сообщения.

## Возможности

- загрузка лог-файла через браузер;
- сохранение файла на сервере;
- выдача уникальной ссылки на просмотр;
- красивое отображение логов в веб-интерфейсе;
- подсветка сообщений по уровню приоритета;
- страница-заглушка, если файл не найден;
- кнопки для быстрой навигации по длинным логам.

## Формат входных данных

Поддерживаются логи в формате JSON-lines, где каждая строка — это отдельный JSON-объект.

Например, такой вывод можно получить через `journalctl`:
````bash
journalctl --since="2026-04-13 23:50:53" --until="2026-04-13 23:54" --output=json
````

## Требования

### Для сборки

- CMake 3.26+
- C++20
- `fmt`
- `cpp-httplib`
- `nlohmann-json`

````bash
apt install -y build-essential cmake libfmt-dev nlohmann-json3-dev dpkg-dev libcpp-httplib-dev
````

## Сборка из исходников

```bash 
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr ..
cmake --build . -j$(nproc)
cmake --install .
```

## Сборка DEB

Пакет собирается через CPack:

```bash 
cd build cpack -G DEB
apt install -y ./log-view_<версия>_amd64.deb
```

После установки будут размещены:

- бинарник: `/usr/bin/log-view`
- конфиг(Настройки): `/etc/log-view/cfg.json`
- unit-файл systemd: `/usr/lib/systemd/system/log-view.service`

### Параметры(cfg.json)

````json
{
"max_storage_size":  5368709120,
"port": 80,
"log_level": 6,
"storage_path": "/var/log-view/storage/"
}
````

- `port` — порт, на котором запускается HTTP-сервер
- `log_level` — уровень логирования для `syslog`
- `max_storage_size` — максимальный размер хранилища в байтах
- `storage_path` — путь к хранилищу



Возможные уровнилогирования:

- `LOG_EMERG` — 0
- `LOG_ALERT` — 1
- `LOG_CRIT` — 2
- `LOG_ERR` — 3
- `LOG_WARNING` — 4
- `LOG_NOTICE` — 5
- `LOG_INFO` — 6
- `LOG_DEBUG` — 7

## Запуск

После установки сервис можно запускать так:

```bash 
systemctl daemon-reload
systemctl enable log-view
systemctl start log-view
```

Проверка статуса:

```bash 
systemctl status log-view
```

По умолчанию сервер доступен на: http://localhost:8080

## Логирование

Приложение пишет сообщения в `syslog`.

Используются уровни:

- `LOG_ERR`
- `LOG_INFO`
- `LOG_NOTICE`
- `LOG_DEBUG`

Уровень фильтрации можно настраивать в коде через `setlogmask()`.