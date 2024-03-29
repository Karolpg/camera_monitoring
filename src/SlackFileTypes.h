//
// The MIT License (MIT)
//
// Copyright 2020 Karolpg
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation the rights to #use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR #COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#pragma once

enum class SlackFileType {
    autoType,     // Auto Detect Type
    text,         // Plain Text
    ai,           // Illustrator File
    apk,          // APK
    applescript,  // AppleScript
    binary,       // Binary
    bmp,          // Bitmap
    boxnote,      // BoxNote
    c,            // C
    csharp,       // C#
    cpp,          // C++
    css,          // CSS
    csv,          // CSV
    clojure,      // Clojure
    coffeescript, // CoffeeScript
    cfm,          // ColdFusion
    d,            // D
    dart,         // Dart
    diff,         // Diff
    doc,          // Word Document
    docx,         // Word document
    dockerfile,   // Docker
    dotx,         // Word template
    email,        // Email
    eps,          // EPS
    epub,         // EPUB
    erlang,       // Erlang
    fla,          // Flash FLA
    flv,          // Flash video
    fsharp,       // F#
    fortran,      // Fortran
    gdoc,         // GDocs Document
    gdraw,        // GDocs Drawing
    gif,          // GIF
    go,           // Go
    gpres,        // GDocs Presentation
    groovy,       // Groovy
    gsheet,       // GDocs Spreadsheet
    gzip,         // GZip
    html,         // HTML
    handlebars,   // Handlebars
    haskell,      // Haskell
    haxe,         // Haxe
    indd,         // InDesign Document
    java,         // Java
    javascript,   // JavaScript/JSON
    jpg,          // JPEG
    keynote,      // Keynote Document
    kotlin,       // Kotlin
    latex,        // LaTeX/sTeX
    lisp,         // Lisp
    lua,          // Lua
    m4a,          // MPEG 4 audio
    markdown,     // Markdown (raw)
    matlab,       // MATLAB
    mhtml,        // MHTML
    mkv,          // Matroska video
    mov,          // QuickTime video
    mp3,          // mp4
    mp4,          // MPEG 4 video
    mpg,          // MPEG video
    mumps,        // MUMPS
    numbers,      // Numbers Document
    nzb,          // NZB
    objc,         // Objective-C
    ocaml,        // OCaml
    odg,          // OpenDocument Drawing
    odi,          // OpenDocument Image
    odp,          // OpenDocument Presentation
    ods,          // OpenDocument Spreadsheet
    odt,          // OpenDocument Text
    ogg,          // Ogg Vorbis
    ogv,          // Ogg video
    pages,        // Pages Document
    pascal,       // Pascal
    pdf,          // PDF
    perl,         // Perl
    php,          // PHP
    pig,          // Pig
    png,          // PNG
    post,         // Slack Post
    powershell,   // PowerShell
    ppt,          // PowerPoint presentation
    pptx,         // PowerPoint presentation
    psd,          // Photoshop Document
    puppet,       // Puppet
    python,       // Python
    qtz,          // Quartz Composer Composition
    r,            // R
    rtf,          // Rich Text File
    ruby,         // Ruby
    rust,         // Rust
    sql,          // SQL
    sass,         // Sass
    scala,        // Scala
    scheme,       // Scheme
    sketch,       // Sketch File
    shell,        // Shell
    smalltalk,    // Smalltalk
    svg,          // SVG
    swf,          // Flash SWF
    swift,        // Swift
    tar,          // Tarball
    tiff,         // TIFF
    tsv,          // TSV
    vb,           // VB.NET
    vbscript,     // VBScript
    vcard,        // vCard
    velocity,     // Velocity
    verilog,      // Verilog
    wav,          // Waveform audio
    webm,         // WebM
    wmv,          // Windows Media Video
    xls,          // Excel spreadsheet
    xlsx,         // Excel spreadsheet
    xlsb,         // Excel Spreadsheet (Binary, Macro Enabled)
    xlsm,         // Excel Spreadsheet (Macro Enabled)
    xltx,         // Excel template
    xml,          // XML
    yaml,         // YAML
    zip,          // Zip
    count_SlackFileType
};

const char* slackFileTypeStr(SlackFileType type);
