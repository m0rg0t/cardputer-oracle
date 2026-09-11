const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../site/language.js'), 'utf8');
const { chooseLanguage } = require('../site/language.js');
assert.equal(chooseLanguage(null, ['ru-RU', 'en-US']), 'ru');
assert.equal(chooseLanguage(null, ['zh-CN']), 'zh');
assert.equal(chooseLanguage(null, ['zh-Hant-TW']), 'zh');
assert.equal(chooseLanguage(null, ['fr-FR', 'ru']), 'ru');
assert.equal(chooseLanguage(null, ['de-DE']), 'en');
assert.equal(chooseLanguage('en', ['ru']), 'en');
assert.equal(chooseLanguage('invalid', ['zh']), 'zh');
assert.equal(chooseLanguage(null, []), 'en');
function browser(entry, saved, blocked = false) {
  const state = { redirect: null, saved, ready: null };
  const link = { href: 'https://example.test/cardputer-oracle/zh/', dataset: { language: 'zh' },
    addEventListener: (_, fn) => { state.click = fn; } };
  vm.runInNewContext(source, { URL,
    navigator: { languages: ['ru-RU'] },
    location: { href: 'https://example.test/cardputer-oracle/' + (entry === 'auto' ? '' : 'en/'),
      search: '?preview=1', hash: '#examples', replace: value => { state.redirect = value; } },
    localStorage: { getItem: () => { if (blocked) throw Error('blocked'); return saved; },
      setItem: (_, value) => { if (blocked) throw Error('blocked'); state.saved = value; } },
    document: { documentElement: { dataset: { languageEntry: entry } },
      addEventListener: (_, fn) => { state.ready = fn; }, querySelectorAll: () => [link] }
  });
  state.ready(); state.click(); state.link = link.href; return state;
}
assert.equal(browser('auto', null).redirect, 'https://example.test/cardputer-oracle/ru/?preview=1#examples');
assert.equal(browser('auto', 'zh').redirect, 'https://example.test/cardputer-oracle/zh/?preview=1#examples');
assert.equal(browser('auto', 'en', true).redirect, 'https://example.test/cardputer-oracle/ru/?preview=1#examples');
assert.equal(browser('explicit', 'ru').redirect, null);
assert.equal(browser('explicit', null).saved, 'zh');
assert.equal(browser('explicit', null).link, 'https://example.test/cardputer-oracle/zh/?preview=1#examples');
assert.doesNotThrow(() => browser('explicit', null, true));
console.log('PASS: regional languages, fallback, saved preference, blocked storage, explicit URLs, project subpath and anchors');
