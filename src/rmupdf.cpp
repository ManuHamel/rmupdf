// [[Rcpp::plugins(cpp17)]]
#include <Rcpp.h>

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

using namespace Rcpp;


struct MuPdfDocHolder {
  fz_context* ctx;
  fz_document* doc;
  pdf_document* pdfdoc;
  std::string path;

  MuPdfDocHolder() : ctx(nullptr), doc(nullptr), pdfdoc(nullptr) {}

  ~MuPdfDocHolder() {
    if (doc != nullptr && ctx != nullptr) {
      fz_drop_document(ctx, doc);
      doc = nullptr;
      pdfdoc = nullptr;
    }
    if (ctx != nullptr) {
      fz_drop_context(ctx);
      ctx = nullptr;
    }
  }
};

static MuPdfDocHolder* get_holder(SEXP xp) {
  Rcpp::XPtr<MuPdfDocHolder> ptr(xp);
  if (!ptr || ptr->ctx == nullptr || ptr->doc == nullptr || ptr->pdfdoc == nullptr) {
    Rcpp::stop("Invalid MuPDF document or already closed.");
  }
  return ptr.get();
}

static void check_pdf_doc(MuPdfDocHolder* holder) {
  if (holder == nullptr || holder->ctx == nullptr || holder->doc == nullptr || holder->pdfdoc == nullptr) {
    Rcpp::stop("Invalid PDF document.");
  }
}

static int page_to_index(MuPdfDocHolder* holder, int page_number) {
  check_pdf_doc(holder);

  if (page_number < 1) {
    Rcpp::stop("page must be >= 1.");
  }

  int n = 0;
  fz_try(holder->ctx) {
    n = fz_count_pages(holder->ctx, holder->doc);
  }
  fz_catch(holder->ctx) {
    Rcpp::stop("Unable to get the number of pages.");
  }

  if (page_number > n) {
    Rcpp::stop("Page out of bounds.");
  }

  return page_number - 1;
}

static std::string annot_type_to_string(int t) {
  switch (t) {
  case PDF_ANNOT_TEXT: return "Text";
  case PDF_ANNOT_LINK: return "Link";
  case PDF_ANNOT_FREE_TEXT: return "FreeText";
  case PDF_ANNOT_LINE: return "Line";
  case PDF_ANNOT_SQUARE: return "Square";
  case PDF_ANNOT_CIRCLE: return "Circle";
  case PDF_ANNOT_POLYGON: return "Polygon";
  case PDF_ANNOT_POLY_LINE: return "PolyLine";
  case PDF_ANNOT_HIGHLIGHT: return "Highlight";
  case PDF_ANNOT_UNDERLINE: return "Underline";
  case PDF_ANNOT_SQUIGGLY: return "Squiggly";
  case PDF_ANNOT_STRIKE_OUT: return "StrikeOut";
  case PDF_ANNOT_STAMP: return "Stamp";
  case PDF_ANNOT_CARET: return "Caret";
  case PDF_ANNOT_INK: return "Ink";
  case PDF_ANNOT_POPUP: return "Popup";
  case PDF_ANNOT_FILE_ATTACHMENT: return "FileAttachment";
  case PDF_ANNOT_SOUND: return "Sound";
  case PDF_ANNOT_MOVIE: return "Movie";
  case PDF_ANNOT_WIDGET: return "Widget";
  case PDF_ANNOT_SCREEN: return "Screen";
  case PDF_ANNOT_PRINTER_MARK: return "PrinterMark";
  case PDF_ANNOT_TRAP_NET: return "TrapNet";
  case PDF_ANNOT_WATERMARK: return "Watermark";
  case PDF_ANNOT_3D: return "Model3D";
  case PDF_ANNOT_REDACT: return "Redact";
  default: return "Unknown";
  }
}


static std::string get_meta_string(MuPdfDocHolder* holder, const char* key) {
  check_pdf_doc(holder);

  char buf[4096];
  buf[0] = '\0';

  fz_try(holder->ctx) {
    int n = fz_lookup_metadata(holder->ctx, holder->doc, key, buf, sizeof(buf));
    if (n < 0) {
      return "";
    }
  }
  fz_catch(holder->ctx) {
    return "";
  }

  return std::string(buf);
}

static void ensure_info_dict(MuPdfDocHolder* holder, pdf_obj** trailer_out, pdf_obj** info_out) {
  check_pdf_doc(holder);

  pdf_obj* trailer = nullptr;
  pdf_obj* info = nullptr;

  fz_try(holder->ctx) {
    trailer = pdf_trailer(holder->ctx, holder->pdfdoc);
    info = pdf_dict_get(holder->ctx, trailer, PDF_NAME(Info));

    if (!info || !pdf_is_dict(holder->ctx, info)) {
      info = pdf_new_dict(holder->ctx, holder->pdfdoc, 8);
      pdf_dict_put(holder->ctx, trailer, PDF_NAME(Info), info);
      pdf_drop_obj(holder->ctx, info);
      info = pdf_dict_get(holder->ctx, trailer, PDF_NAME(Info));
    }
  }
  fz_catch(holder->ctx) {
    Rcpp::stop("Unable to access the Info dictionary.");
  }

  *trailer_out = trailer;
  *info_out = info;
}

static void set_info_string(MuPdfDocHolder* holder, pdf_obj* info, pdf_obj* key, const std::string& value) {
  fz_try(holder->ctx) {
    pdf_dict_put_text_string(holder->ctx, info, key, value.c_str());
  }
  fz_catch(holder->ctx) {
    Rcpp::stop("Unable to write metadata.");
  }
}


// [[Rcpp::export]]
SEXP pdf_open_mupdf(std::string path) {
  MuPdfDocHolder* holder = new MuPdfDocHolder();

  try {
    holder->ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (holder->ctx == nullptr) {
      delete holder;
      Rcpp::stop("Unable to create the MuPDF context.");
    }

    fz_register_document_handlers(holder->ctx);

    fz_try(holder->ctx) {
      holder->doc = fz_open_document(holder->ctx, path.c_str());
      holder->pdfdoc = pdf_specifics(holder->ctx, holder->doc);
    }
    fz_catch(holder->ctx) {
      delete holder;
      Rcpp::stop("MuPDF error while opening PDF.");
    }

    if (holder->pdfdoc == nullptr) {
      delete holder;
      Rcpp::stop("The opened document is not a PDF.");
    }

    holder->path = path;

    Rcpp::XPtr<MuPdfDocHolder> xp(holder, true);
    return xp;

  } catch (const std::exception& e) {
    delete holder;
    Rcpp::stop(std::string("Standard exception while opening PDF: ") + e.what());
  } catch (...) {
    delete holder;
    Rcpp::stop("Unknown error while opening PDF.");
  }
}


// [[Rcpp::export]]
void pdf_close_mupdf(SEXP doc_xptr) {
  MuPdfDocHolder* holder = get_holder(doc_xptr);

  if (holder->doc != nullptr && holder->ctx != nullptr) {
    fz_drop_document(holder->ctx, holder->doc);
    holder->doc = nullptr;
    holder->pdfdoc = nullptr;
  }

  if (holder->ctx != nullptr) {
    fz_drop_context(holder->ctx);
    holder->ctx = nullptr;
  }
}


// [[Rcpp::export]]
int pdf_pages_mupdf(SEXP doc_xptr) {
  MuPdfDocHolder* holder = get_holder(doc_xptr);

  int n = 0;
  fz_try(holder->ctx) {
    n = fz_count_pages(holder->ctx, holder->doc);
  }
  fz_catch(holder->ctx) {
    Rcpp::stop("Unable to get the number of pages.");
  }

  return n;
}


// [[Rcpp::export]]
Rcpp::List pdf_get_metadata_mupdf(SEXP doc_xptr) {
  MuPdfDocHolder* holder = get_holder(doc_xptr);

  std::string title    = get_meta_string(holder, FZ_META_INFO_TITLE);
  std::string author   = get_meta_string(holder, FZ_META_INFO_AUTHOR);
  std::string subject  = get_meta_string(holder, FZ_META_INFO_SUBJECT);
  std::string keywords = get_meta_string(holder, FZ_META_INFO_KEYWORDS);
  std::string creator  = get_meta_string(holder, FZ_META_INFO_CREATOR);
  std::string producer = get_meta_string(holder, FZ_META_INFO_PRODUCER);

  return Rcpp::List::create(
    _["title"]    = title.empty()    ? NA_STRING : Rf_mkCharCE(title.c_str(), CE_UTF8),
    _["author"]   = author.empty()   ? NA_STRING : Rf_mkCharCE(author.c_str(), CE_UTF8),
    _["subject"]  = subject.empty()  ? NA_STRING : Rf_mkCharCE(subject.c_str(), CE_UTF8),
    _["keywords"] = keywords.empty() ? NA_STRING : Rf_mkCharCE(keywords.c_str(), CE_UTF8),
    _["creator"]  = creator.empty()  ? NA_STRING : Rf_mkCharCE(creator.c_str(), CE_UTF8),
    _["producer"] = producer.empty() ? NA_STRING : Rf_mkCharCE(producer.c_str(), CE_UTF8)
  );
}


// [[Rcpp::export]]
void pdf_set_metadata_mupdf(SEXP doc_xptr,
                            Rcpp::Nullable<std::string> title = R_NilValue,
                            Rcpp::Nullable<std::string> author = R_NilValue,
                            Rcpp::Nullable<std::string> subject = R_NilValue,
                            Rcpp::Nullable<std::string> keywords = R_NilValue,
                            Rcpp::Nullable<std::string> creator = R_NilValue,
                            Rcpp::Nullable<std::string> producer = R_NilValue) {
  MuPdfDocHolder* holder = get_holder(doc_xptr);

  pdf_obj* trailer = nullptr;
  pdf_obj* info = nullptr;
  ensure_info_dict(holder, &trailer, &info);
  (void)trailer;

  if (title.isNotNull()) {
    set_info_string(holder, info, PDF_NAME(Title), Rcpp::as<std::string>(title));
  }
  if (author.isNotNull()) {
    set_info_string(holder, info, PDF_NAME(Author), Rcpp::as<std::string>(author));
  }
  if (subject.isNotNull()) {
    set_info_string(holder, info, PDF_NAME(Subject), Rcpp::as<std::string>(subject));
  }
  if (keywords.isNotNull()) {
    set_info_string(holder, info, PDF_NAME(Keywords), Rcpp::as<std::string>(keywords));
  }
  if (creator.isNotNull()) {
    set_info_string(holder, info, PDF_NAME(Creator), Rcpp::as<std::string>(creator));
  }
  if (producer.isNotNull()) {
    set_info_string(holder, info, PDF_NAME(Producer), Rcpp::as<std::string>(producer));
  }
}


// [[Rcpp::export]]
Rcpp::DataFrame pdf_list_annots_mupdf(SEXP doc_xptr, int page) {
  MuPdfDocHolder* holder = get_holder(doc_xptr);
  int page_index = page_to_index(holder, page);

  IntegerVector ids;
  CharacterVector subtype;
  CharacterVector title;
  CharacterVector contents;
  NumericVector x;
  NumericVector y;
  NumericVector width;
  NumericVector height;

  fz_page* fpage = nullptr;
  pdf_page* ppage = nullptr;

  fz_try(holder->ctx) {
    fpage = fz_load_page(holder->ctx, holder->doc, page_index);
    ppage = pdf_page_from_fz_page(holder->ctx, fpage);

    int id = 1;
    for (pdf_annot* a = pdf_first_annot(holder->ctx, ppage);
         a != nullptr;
         a = pdf_next_annot(holder->ctx, a), ++id) {

      int t = pdf_annot_type(holder->ctx, a);
      fz_rect r = pdf_bound_annot(holder->ctx, a);

      const char* author_c = "";
      const char* contents_c = "";

      author_c = pdf_annot_author(holder->ctx, a);
      contents_c = pdf_annot_contents(holder->ctx, a);

      ids.push_back(id);
      subtype.push_back(annot_type_to_string(t));
      title.push_back(author_c ? author_c : "");
      contents.push_back(contents_c ? contents_c : "");
      x.push_back(r.x0);
      y.push_back(r.y0);
      width.push_back(r.x1 - r.x0);
      height.push_back(r.y1 - r.y0);
    }
  }
  fz_always(holder->ctx) {
    if (fpage != nullptr) {
      fz_drop_page(holder->ctx, fpage);
    }
  }
  fz_catch(holder->ctx) {
    Rcpp::stop("Unable to list annotations.");
  }

  return Rcpp::DataFrame::create(
    _["id"] = ids,
    _["subtype"] = subtype,
    _["title"] = title,
    _["contents"] = contents,
    _["x"] = x,
    _["y"] = y,
    _["width"] = width,
    _["height"] = height,
    _["stringsAsFactors"] = false
  );
}


// [[Rcpp::export]]
void pdf_add_highlight_mupdf(SEXP doc_xptr,
                             int page,
                             double x,
                             double y,
                             double width,
                             double height,
                             Rcpp::Nullable<std::string> contents = R_NilValue,
                             double r = 1.0,
                             double g = 1.0,
                             double b = 0.0,
                             bool fallback_square = true) {
  MuPdfDocHolder* holder = get_holder(doc_xptr);
  int page_index = page_to_index(holder, page);

  fz_page* fpage = nullptr;
  pdf_page* ppage = nullptr;
  pdf_annot* annot = nullptr;

  fz_try(holder->ctx) {
    fpage = fz_load_page(holder->ctx, holder->doc, page_index);
    ppage = pdf_page_from_fz_page(holder->ctx, fpage);

    fz_rect rect;
    rect.x0 = static_cast<float>(x);
    rect.y0 = static_cast<float>(y);
    rect.x1 = static_cast<float>(x + width);
    rect.y1 = static_cast<float>(y + height);

    annot = pdf_create_annot(holder->ctx, ppage, PDF_ANNOT_HIGHLIGHT);
    pdf_set_annot_rect(holder->ctx, annot, rect);

    pdf_obj* quadpoints = pdf_new_array(holder->ctx, holder->pdfdoc, 8);
    pdf_array_push_real(holder->ctx, quadpoints, rect.x0);
    pdf_array_push_real(holder->ctx, quadpoints, rect.y1);
    pdf_array_push_real(holder->ctx, quadpoints, rect.x1);
    pdf_array_push_real(holder->ctx, quadpoints, rect.y1);
    pdf_array_push_real(holder->ctx, quadpoints, rect.x0);
    pdf_array_push_real(holder->ctx, quadpoints, rect.y0);
    pdf_array_push_real(holder->ctx, quadpoints, rect.x1);
    pdf_array_push_real(holder->ctx, quadpoints, rect.y0);

    pdf_obj* annot_obj = pdf_annot_obj(holder->ctx, annot);
    pdf_dict_put(holder->ctx, annot_obj, PDF_NAME(QuadPoints), quadpoints);
    pdf_drop_obj(holder->ctx, quadpoints);

    float color[3];
    color[0] = static_cast<float>(r);
    color[1] = static_cast<float>(g);
    color[2] = static_cast<float>(b);
    pdf_set_annot_color(holder->ctx, annot, 3, color);

    if (contents.isNotNull()) {
      std::string txt = Rcpp::as<std::string>(contents);
      pdf_set_annot_contents(holder->ctx, annot, txt.c_str());
    }

    pdf_update_annot(holder->ctx, annot);
  }

  fz_catch(holder->ctx) {
    if (!fallback_square) {
      if (fpage != nullptr) {
        fz_drop_page(holder->ctx, fpage);
      }
      Rcpp::stop("Unable to add the highlight.");
    }

    try {
      if (fpage != nullptr) {
        fz_drop_page(holder->ctx, fpage);
        fpage = nullptr;
      }

      fpage = fz_load_page(holder->ctx, holder->doc, page_index);
      ppage = pdf_page_from_fz_page(holder->ctx, fpage);

      fz_rect rect;
      rect.x0 = static_cast<float>(x);
      rect.y0 = static_cast<float>(y);
      rect.x1 = static_cast<float>(x + width);
      rect.y1 = static_cast<float>(y + height);

      annot = pdf_create_annot(holder->ctx, ppage, PDF_ANNOT_SQUARE);
      pdf_set_annot_rect(holder->ctx, annot, rect);

      float color[3];
      color[0] = static_cast<float>(r);
      color[1] = static_cast<float>(g);
      color[2] = static_cast<float>(b);
      pdf_set_annot_color(holder->ctx, annot, 3, color);

      if (contents.isNotNull()) {
        std::string txt = Rcpp::as<std::string>(contents);
        pdf_set_annot_contents(holder->ctx, annot, txt.c_str());
      }

      pdf_update_annot(holder->ctx, annot);
    } catch (...) {
      if (fpage != nullptr) {
        fz_drop_page(holder->ctx, fpage);
      }
      Rcpp::stop("Unable to add the highlight, even with Square fallback.");
    }
  }

  if (fpage != nullptr) {
    fz_drop_page(holder->ctx, fpage);
  }
}


// [[Rcpp::export]]
void pdf_save_mupdf(SEXP doc_xptr, std::string outpath, bool incremental = false) {
  MuPdfDocHolder* holder = get_holder(doc_xptr);

  pdf_write_options opts = pdf_default_write_options;
  opts.do_incremental = incremental ? 1 : 0;
  opts.do_pretty = 0;
  opts.do_ascii = 0;
  opts.do_compress = 1;
  opts.do_compress_images = 1;
  opts.do_compress_fonts = 1;
  opts.do_garbage = 3;

  fz_try(holder->ctx) {
    pdf_save_document(holder->ctx, holder->pdfdoc, outpath.c_str(), &opts);
  }
  fz_catch(holder->ctx) {
    Rcpp::stop("Unable to save the PDF.");
  }
}
