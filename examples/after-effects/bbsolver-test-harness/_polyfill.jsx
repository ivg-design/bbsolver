// _polyfill.jsx — JSON guard for very old ExtendScript runtimes.
//
// AE CC 2014 and later expose a native global JSON object. The harness only
// supports those versions. If JSON is missing, we alert and abort cleanly
// rather than carry the full Douglas Crockford json2.js polyfill, whose
// regex literals contain raw Unicode characters that ExtendScript's parser
// rejects in some host configurations.

if (typeof JSON === 'undefined' || typeof JSON.stringify !== 'function') {
    alert(
        'bbsolver-test-harness requires After Effects CC 2014 or later.\n\n' +
        'Native JSON is not available in this AE version.'
    );
    // Stub so subsequent code doesn't crash on undefined.
    JSON = {
        stringify: function () { return ''; },
        parse:     function () { return null; }
    };
}
