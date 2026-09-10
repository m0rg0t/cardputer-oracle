# Разработка

Python 3.10+, Pillow из requirements.txt, PlatformIO и C++17-компилятор для необязательных host-тестов. Точные версии платформы и библиотек закреплены в firmware/platformio.ini.

~~~sh
python -m unittest discover -s tools -p test_pack.py
python tools/run_native_tests.py
cd firmware
pio run -e oracle-adv
~~~

Native-тесты используют тот же core и парсеры и собираются без исключений C++, как прошивка. Они не эмулируют SD, I2S, ADC и BLE-радио.

## Симулятор

Нужен SDL2: на macOS доступен через Homebrew, на Debian/Ubuntu — libsdl2-dev.

~~~sh
cd firmware
pio run -e native-sim
cd ..
firmware/.pio/build/native-sim/program --art sdcard/ORACLE/images/oracle.jpg
~~~

Снимки без окна:

~~~sh
mkdir -p docs/screens
SDL_VIDEODRIVER=dummy firmware/.pio/build/native-sim/program --shots docs/screens --art sdcard/ORACLE/images/oracle.jpg
python tools/render_screens.py
~~~

Генерируемые отчёты исключены из Git. Выбранные иллюстрации документации находятся в docs/media.

## Слои

- core.cpp: валидация карточек, темы, перемешивание и состояния приложения.
- draw.cpp: общий для симулятора и устройства экран 240×135.
- main.cpp: M5Cardputer, клавиатура, SD, батарея и воспроизведение.
- sound.cpp и аудиохелперы: эффекты и потоковое воспроизведение WAV.
- tarot.cpp / tarot_api.cpp / tarot_client.cpp: вопрос, расклад, формат ответа, Wi-Fi и HTTPS.
- print_protocol.cpp / tarot_receipt.cpp / bq95_printer.cpp: растр и BLE.

Компактный индекс содержит ID и тему; все JSON при нажатии не загружаются. Выбранная карточка проверяется после чтения, чтобы устаревший индекс не смешивал темы.

## M5Apps и радио

Wi-Fi запускается без сохранения конфигурации в стандартном NVS: настройки живут на SD. Для NimBLE 1.4.3 tools/nimble_m5apps.py создаёт отдельную копию NimBLEDevice.cpp без обязательной инициализации и стирания NVS. Bluetooth-сопряжения не сохраняются. Исходники загруженной библиотеки не меняются.

При обновлении NimBLE адаптация требует повторной проверки. tools/check_nimble_storage.py проверяет ссылки уже скомпилированной библиотеки. Стирать apps_nvs для работы оракула не нужно.

BLE освобождается перед Таро и запускается заново для печати. Частично переданный чек автоматически не повторяется.

## Выпуск пакета

~~~sh
python tools/package_demo.py
python tools/validate_release.py
~~~

Проверка работает без приватного конфига и старых ZIP. Если release/private/config.json существует, его токен и пароли дополнительно сверяются с публичными артефактами без вывода значений.

Артефакты release не добавляются в Git; для распространения подходят вложения GitHub Releases. Создание релиза автоматически не выполняется.

При изменении версии обновить VERSION и видимый номер в draw.cpp / diagnostics.cpp. Пересобрать, проверить лимит M5Apps и обновить инструкции. Сайт читает версию из VERSION.

Личные диагностические материалы и прежний адресный установщик SD сохранены в local; новому клону они не требуются.
