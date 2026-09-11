/* Direct language URLs are explicit. Only the entry page detects preferences. */
(function () {
  "use strict";
  const supported = ["en", "ru", "zh"];
  function chooseLanguage(saved, preferred) {
    if (supported.includes(saved)) return saved;
    for (const value of preferred || []) {
      const base = String(value).toLowerCase().split(/[-_]/)[0];
      if (supported.includes(base)) return base;
    }
    return "en";
  }
  if (typeof module !== "undefined" && module.exports) module.exports = { chooseLanguage };
  if (typeof document === "undefined") return;
  const storageKey = "oracle-site-language";
  if (document.documentElement.dataset.languageEntry === "auto") {
    let saved = null;
    try { saved = localStorage.getItem(storageKey); } catch (_) { /* Optional storage. */ }
    const language = chooseLanguage(saved, navigator.languages || [navigator.language]);
    const target = new URL(language + "/", new URL("./", location.href));
    target.search = location.search;
    target.hash = location.hash;
    location.replace(target.href);
  }
  document.addEventListener("DOMContentLoaded", function () {
    document.querySelectorAll("a[data-language]").forEach(function (link) {
      function preserveSection() {
        const target = new URL(link.href);
        target.search = location.search;
        target.hash = location.hash;
        link.href = target.href;
      }
      preserveSection();
      link.addEventListener("click", function () {
        preserveSection();
        try { localStorage.setItem(storageKey, link.dataset.language); } catch (_) { /* Optional. */ }
      });
    });
  });
})();
