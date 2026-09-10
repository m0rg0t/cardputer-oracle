# GitHub Pages

Исходники — site/index.html и site/style.css. Генератор tools/build_site.py использует стандартную библиотеку Python 3.10+. Примеры берутся из настоящей колоды, включая три готовых WAV.

~~~sh
python tools/build_site.py
python -m http.server 8765 --bind 127.0.0.1 --directory build/site
~~~

Открыть http://127.0.0.1:8765/. При изменениях повторить build_site.py. Для ссылки на репозиторий передать --repository OWNER/REPOSITORY.

Генератор проверяет локальные ссылки. На выходе только 12 публичных файлов; приватный конфиг, папка SD целиком, журналы и прошивки не копируются. Сайт без JavaScript, трекеров, внешних шрифтов и API. Аудио включается посетителем.

## Публикация

1. Создать репозиторий GitHub и отправить локальную main.
2. Settings → Pages → Source → GitHub Actions.
3. Actions → Deploy GitHub Pages → Run workflow.
4. Дождаться build и deploy. URL появится у environment github-pages.

Публикация только через workflow_dispatch. Checks на push / pull request проверяют сайт без развёртывания. Все пути относительные и подходят для OWNER.github.io/REPOSITORY/.

Имя текущего репозитория передаётся генератору через переменную окружения. Deploy job получает pages: write и id-token: write. API-ключи OpenAI, LiteLLM и ElevenLabs в GitHub Secrets для сайта не нужны.

[Официальная документация workflow](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).
