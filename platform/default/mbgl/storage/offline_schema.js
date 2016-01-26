// To regenerate:
// (cd platform/default/mbgl/storage && node offline_schema.js)

var fs = require('fs');
var readline = require('readline');

var lineReader = readline.createInterface({
    input: fs.createReadStream('offline_schema.sql')
});

var lines = [
    "/* THIS IS A GENERATED FILE; EDIT offline_schema.sql INSTEAD */",
    "static const char * schema = ",
];

lineReader
    .on('line', function (line) {
        lines.push('"' + line.replace(/ *--.*/, '') + '" \\');
    })
    .on('close', function () {
        lines.push(';');
        fs.writeFileSync('offline_schema.cpp.include', lines.join('\n'));
    });
