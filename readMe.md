# rmupdf

**rmupdf** is an R package powered by **Rcpp** that provides bindings to the **MuPDF** library for working with PDF files.

It allows you to:

- open PDF documents
- count pages
- read and modify metadata
- list annotations
- add annotations (highlight / rectangle)
- save modified documents

---

## ✨ Features

- Simple R interface to MuPDF
- High performance (C++ via Rcpp)
- Support for PDF annotations
- Compatible with Windows (Rtools + UCRT)

---

## 📦 Installation

### Requirements

You need:

- R (>= 4.3 recommended)
- Rtools (on Windows)
- MuPDF installed (headers + libraries)

---

### MuPDF configuration (Windows)

If you are using Rtools45, MuPDF is typically located at:
