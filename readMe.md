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

### Code for installation 

Here is an example of how you can install MuPDF on Windows with a R code : 

```
run_cmd <- function(cmd, args = character(), error_msg = NULL, echo = TRUE) {
  if (echo) {
    message(">> ", paste(c(cmd, args), collapse = " "))
  }
  status <- system2(cmd, args = args)
  if (!identical(status, 0L)) {
    stop(if (is.null(error_msg)) paste("Command failed:", cmd) else error_msg, call. = FALSE)
  }
  invisible(TRUE)
}

check_cmd <- function(cmd) {
  nzchar(Sys.which(cmd))
}

write_mupdf_env <- function(include_dir, lib_dir, scope = c("session", "renviron")) {
  scope <- match.arg(scope)

  Sys.setenv(MUPDF_INCLUDE = include_dir, MUPDF_LIB = lib_dir)

  if (scope == "renviron") {
    renv <- path.expand("~/.Renviron")
    old <- if (file.exists(renv)) readLines(renv, warn = FALSE) else character()

    old <- old[!grepl("^MUPDF_INCLUDE=", old)]
    old <- old[!grepl("^MUPDF_LIB=", old)]

    new_lines <- c(
      old,
      sprintf('MUPDF_INCLUDE="%s"', normalizePath(include_dir, winslash = "/", mustWork = FALSE)),
      sprintf('MUPDF_LIB="%s"', normalizePath(lib_dir, winslash = "/", mustWork = FALSE))
    )
    writeLines(new_lines, renv)
    message("Variables added to ~/.Renviron")
  }

  message("MUPDF_INCLUDE = ", Sys.getenv("MUPDF_INCLUDE"))
  message("MUPDF_LIB     = ", Sys.getenv("MUPDF_LIB"))
}

install_mupdf_linux <- function(use_sudo = TRUE, persist = TRUE) {
  if (!check_cmd("apt-get")) {
    stop("This Linux function is intended for Debian/Ubuntu (apt-get).", call. = FALSE)
  }

  sudo <- if (use_sudo) "sudo" else ""
  apt_pkgs <- c("mupdf", "libmupdf-dev")

  if (use_sudo) {
    run_cmd("sudo", c("apt-get", "update"), "apt-get update a échoué.")
    run_cmd("sudo", c("apt-get", "install", "-y", apt_pkgs),
            "APT installation of MuPDF failed.")
  } else {
    run_cmd("apt-get", c("update"), "apt-get update a échoué.")
    run_cmd("apt-get", c("install", "-y", apt_pkgs),
            "APT installation of MuPDF failed.")
  }

  include_candidates <- c("/usr/include", "/usr/local/include")
  lib_candidates <- c("/usr/lib/x86_64-linux-gnu", "/usr/lib", "/usr/local/lib")

  include_dir <- include_candidates[file.exists(file.path(include_candidates, "mupdf", "fitz.h"))][1]
  lib_dir <- lib_candidates[
    file.exists(file.path(lib_candidates, "libmupdf.a")) |
      file.exists(file.path(lib_candidates, "libmupdf.so")) |
      file.exists(file.path(lib_candidates, "libmupdf.dll.a"))
  ][1]

  if (is.na(include_dir) || is.na(lib_dir)) {
    warning("MuPDF is installed, but the include/lib paths were not detected automatically.")
    include_dir <- "/usr/include"
    lib_dir <- "/usr/lib"
  }

  write_mupdf_env(include_dir, lib_dir, scope = if (persist) "renviron" else "session")

  invisible(list(include = include_dir, lib = lib_dir))
}

install_mupdf_macos <- function(persist = TRUE) {
  if (!check_cmd("brew")) stop("Homebrew is not available.", call. = FALSE)

  run_cmd("brew", c("install", "mupdf"),
          "Homebrew installation of MuPDF failed.")

  brew_prefix <- trimws(system2("brew", "--prefix", stdout = TRUE))
  mupdf_prefix <- trimws(system2("brew", c("--prefix", "mupdf"), stdout = TRUE))

  include_candidates <- c(
    file.path(mupdf_prefix, "include"),
    file.path(brew_prefix, "include")
  )
  lib_candidates <- c(
    file.path(mupdf_prefix, "lib"),
    file.path(brew_prefix, "lib")
  )

  include_dir <- include_candidates[file.exists(file.path(include_candidates, "mupdf", "fitz.h"))][1]
  lib_dir <- lib_candidates[
    file.exists(file.path(lib_candidates, "libmupdf.a")) |
      file.exists(file.path(lib_candidates, "libmupdf.dylib"))
  ][1]

  if (is.na(include_dir) || is.na(lib_dir)) {
    warning("MuPDF is installed, but the include/lib paths were not detected automatically.")
    include_dir <- file.path(brew_prefix, "include")
    lib_dir <- file.path(brew_prefix, "lib")
  }

  write_mupdf_env(include_dir, lib_dir, scope = if (persist) "renviron" else "session")

  invisible(list(prefix = mupdf_prefix, include = include_dir, lib = lib_dir))
}

install_mupdf_windows_msys2 <- function(rtools_root = "C:/rtools45",
                                        repo = "ucrt64",
                                        install_tools = TRUE,
                                        persist = TRUE) {
  bash <- file.path(rtools_root, "usr", "bin", "bash.exe")
  if (!file.exists(bash)) {
    stop("bash.exe not found in Rtools/MSYS2 : ", bash, call. = FALSE)
  }

  pkgs <- switch(
    repo,
    ucrt64 = c("mingw-w64-ucrt-x86_64-libmupdf",
               if (install_tools) c("mingw-w64-ucrt-x86_64-mupdf",
                                    "mingw-w64-ucrt-x86_64-mupdf-tools")),
    clang64 = c("mingw-w64-clang-x86_64-libmupdf",
                if (install_tools) c("mingw-w64-clang-x86_64-mupdf",
                                     "mingw-w64-clang-x86_64-mupdf-tools")),
    mingw64 = c("mingw-w64-x86_64-libmupdf",
                if (install_tools) c("mingw-w64-x86_64-mupdf",
                                     "mingw-w64-x86_64-mupdf-tools")),
    stop("repo must be 'ucrt64', 'clang64', or 'mingw64'")
  )

  msys_cmd <- sprintf("pacman -S --needed --noconfirm %s", paste(pkgs, collapse = " "))
  run_cmd(bash, c("-lc", shQuote(msys_cmd)),
          "MSYS2 installation of MuPDF failed.")

  prefix <- switch(
    repo,
    ucrt64  = file.path(rtools_root, "ucrt64"),
    clang64 = file.path(rtools_root, "clang64"),
    mingw64 = file.path(rtools_root, "mingw64")
  )

  include_dir <- file.path(prefix, "include")
  lib_dir <- file.path(prefix, "lib")

  # petite vérification utile
  hdr <- file.path(include_dir, "mupdf", "fitz.h")
  if (!file.exists(hdr)) {
    warning("MuPDF header not found at the expected location: ", hdr)
  }

  libs_found <- any(file.exists(file.path(lib_dir, c(
    "libmupdf.a", "libmupdf.dll.a", "mupdf.lib"
  ))))
  if (!libs_found) {
    warning("MuPDF library not found at the expected location: ", lib_dir)
  }

  write_mupdf_env(include_dir, lib_dir, scope = if (persist) "renviron" else "session")

  invisible(list(
    prefix = prefix,
    include = include_dir,
    lib = lib_dir,
    packages = pkgs
  ))
}

install_mupdf <- function(...) {
  sys <- Sys.info()[["sysname"]]

  if (identical(sys, "Linux")) {
    return(install_mupdf_linux(...))
  }
  if (identical(sys, "Darwin")) {
    return(install_mupdf_macos(...))
  }
  if (identical(sys, "Windows")) {
    return(install_mupdf_windows_msys2(...))
  }

  stop("Operating system not automatically supported: ", sys, call. = FALSE)
}

install_mupdf(rtools_root = "C:/rtools45", repo = "ucrt64")
```

## Example

Here is an example of how the R package can be used : 

```
library(rmupdf)

doc <- pdf_open_mupdf("D:/pdf_highlight.pdf")

pdf_pages_mupdf(doc)

pdf_get_metadata_mupdf(doc)

pdf_set_metadata_mupdf(
  doc,
  title = "New Title",
  author = "Emmanauel"
)

ann <- pdf_list_annots_mupdf(doc, page = 1)
print(ann)

pdf_add_highlight_mupdf(
  doc,
  page = 1,
  x = 100,
  y = 120,
  width = 200,
  height = 40,
  contents = "Highlighted zone",
  r = 1,
  g = 1,
  b = 0
)
```

pdf_save_mupdf(doc, "D:/pdf_highlight_Mod.pdf")
pdf_close_mupdf(doc)
