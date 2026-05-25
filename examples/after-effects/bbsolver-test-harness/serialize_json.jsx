// serialize_json.jsx — writeSampleBundleJson
// Requires _polyfill.jsx to be #included before this file (provides JSON).

// Write 'bundle' (a SampleBundle JS object) to 'filepath' as indented JSON.
// Returns true on success, throws on error.
function writeSampleBundleJson(bundle, filepath) {
    var f = new File(filepath);
    f.encoding = 'UTF-8';
    if (!f.open('w')) {
        throw new Error('writeSampleBundleJson: cannot open file for writing: ' + filepath);
    }
    var text;
    try {
        text = JSON.stringify(bundle, null, 2);
    } catch (e) {
        f.close();
        throw new Error('writeSampleBundleJson: JSON serialization failed: ' + e.message);
    }
    f.write(text);
    f.close();
    return true;
}
