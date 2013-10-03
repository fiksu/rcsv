#include <stdbool.h>
#include <ruby.h>

#include "csv.h"

static VALUE rcsv_parse_error; /* class Rcsv::ParseError << StandardError; end */

/* It is useful to know exact row/column positions and field contents where parse-time exception was raised */
#define RAISE_WITH_LOCATION(row, column, contents, fmt, ...) \
  rb_raise(rcsv_parse_error, "[%d:%d '%s'] " fmt, (int)(row), (int)(column), (char *)(contents), ##__VA_ARGS__);

/* String encoding is only available in Ruby 1.9+ */
#ifdef HAVE_RUBY_ENCODING_H

#include <ruby/encoding.h>

#define RB_ENC_FIND_INDEX(encoding) \
  rb_enc_find_index(encoding)

#define ENCODED_STR_NEW(str, length, enc_index)  \
  ({ \
  VALUE _string = rb_str_new(str, length); \
  if (enc_index != -1) { \
    rb_enc_associate_index(_string, enc_index); \
  } \
  _string; \
  })

#else

#define RB_ENC_FIND_INDEX(encoding) \
  ({ \
  rb_raise(rcsv_parse_error, "Character encodings are unavailable in your ruby version!"); \
  -1; \
  })

#define ENCODED_STR_NEW(str, length, enc_index)  \
  rb_str_new(str, length)

#endif

struct rcsv_metadata {
  /* Derived from user-specified options */
  bool row_as_hash;           /* Used to return array of hashes rather than array of arrays */
  bool empty_field_is_nil;    /* Do we convert empty fields to nils? */
  size_t offset_rows;         /* Number of rows to skip before parsing */
  int encoding_index;         /* If available, the encoding index of the original input */

  char * row_conversions;     /* A pointer to string/array of row conversions char specifiers */
  VALUE * only_rows;          /* A pointer to array of row filters */
  VALUE * except_rows;        /* A pointer to array of negative row filters */
  VALUE * row_defaults;       /* A pointer to array of row defaults */
  VALUE * column_names;       /* A pointer to array of column names to be used with hashes */

  /* Pointer options lengths */
  size_t num_row_conversions; /* Number of converter types in row_conversions array */
  size_t num_only_rows;       /* Number of items in only_rows filter */
  size_t num_except_rows;     /* Number of items in except_rows filter */
  size_t num_row_defaults;    /* Number of default values in row_defaults array */
  size_t num_columns;         /* Number of columns detected from column_names.size */

  /* Internal state */
  bool skip_current_row;      /* Used by only_rows and except_rows filters to skip parsing of the row remainder */
  size_t current_col;         /* Current column's index */
  size_t current_row;         /* Current row's index */

  VALUE last_entry;           /* A pointer to the last entry that's going to be appended to result */
  VALUE * result;             /* A pointer to the parsed data */
};

/* Internal callbacks */

/* This procedure is called for every parsed field */
void end_of_field_callback(void * field, size_t field_size, void * data) {
  const char * field_str = (char *)field;
  struct rcsv_metadata * meta = (struct rcsv_metadata *) data;
  char row_conversion = 0;
  VALUE parsed_field;

  /* No need to parse anything until the end of the line if skip_current_row is set */
  if (meta->skip_current_row) {
    return;
  }

  /* Skip the row if its position is less than specifed offset */
  if (meta->current_row < meta->offset_rows) {
    meta->skip_current_row = true;
    return;
  }

  /* Get row conversion char specifier */
  if (meta->current_col < meta->num_row_conversions) {
    row_conversion = (char)meta->row_conversions[meta->current_col];
  }

  /* Convert the field from string into Ruby type specified by row_conversion */
  if (row_conversion != ' ') { /* spacebar skips the column */
    if (field_size == 0) {
      /* Assigning appropriate default value if applicable. */
      if (meta->current_col < meta->num_row_defaults) {
        parsed_field = meta->row_defaults[meta->current_col];
      } else { /* It depends on empty_field_is_nil if we convert empty strings to nils */
        if (meta->empty_field_is_nil || field_str == NULL) {
          parsed_field = Qnil;
        } else {
          parsed_field = ENCODED_STR_NEW("", 0, meta->encoding_index);
        }
      }
    } else {
      if (meta->current_col < meta->num_row_conversions) {
        switch (row_conversion){
          case 's': /* String */
            parsed_field = ENCODED_STR_NEW(field_str, field_size, meta->encoding_index); 
            break;
          case 'i': /* Integer */
            parsed_field = INT2NUM(atol(field_str));
            break;
          case 'f': /* Float */
            parsed_field = rb_float_new(atof(field_str));
            break;
          case 'b': /* TrueClass/FalseClass */
            switch (field_str[0]) {
              case 't':
              case 'T':
              case '1':
                parsed_field = Qtrue;
                break;
              case 'f':
              case 'F':
              case '0':
                parsed_field = Qfalse;
                break;
              default:
                RAISE_WITH_LOCATION(
                  meta->current_row,
                  meta->current_col,
                  field_str,
                  "Bad Boolean value. Valid values are strings where the first character is T/t/1 for true or F/f/0 for false."
                );
            }
            break;
          default:
            RAISE_WITH_LOCATION(
              meta->current_row,
              meta->current_col,
              field_str,
              "Unknown deserializer '%c'.",
              row_conversion
            );
        }
      } else { /* No conversion happens */
        parsed_field = ENCODED_STR_NEW(field_str, field_size, meta->encoding_index); /* field */
      }
    }

    /* Filter by row values listed in meta->only_rows */
    if ((meta->only_rows != NULL) &&
        (meta->current_col < meta->num_only_rows) &&
        (meta->only_rows[meta->current_col] != Qnil) &&
        (!rb_ary_includes(meta->only_rows[meta->current_col], parsed_field))) {
      meta->skip_current_row = true;
      return;
    }

    /* Filter out by row values listed in meta->except_rows */
    if ((meta->except_rows != NULL) &&
        (meta->current_col < meta->num_except_rows) &&
        (meta->except_rows[meta->current_col] != Qnil) &&
        (rb_ary_includes(meta->except_rows[meta->current_col], parsed_field))) {
      meta->skip_current_row = true;
      return;
    }

    /* Assign the value to appropriate hash key if parsing into Hash */
    if (meta->row_as_hash) {
      if (meta->current_col >= meta->num_columns) {
        RAISE_WITH_LOCATION(
          meta->current_row,
          meta->current_col,
          field_str,
          "There are at least %d columns in a row, which is beyond the number of provided column names (%d).",
          (int)meta->current_col + 1,
          (int)meta->num_columns
        );
      } else {
        rb_hash_aset(meta->last_entry, meta->column_names[meta->current_col], parsed_field); /* last_entry[column_names[current_col]] = field */
      }
    } else { /* Parse into Array */
      rb_ary_push(meta->last_entry, parsed_field); /* last_entry << field */
    }
  }

  /* Increment column counter */
  meta->current_col++;
  return;
}

/* This procedure is called for every line ending */
void end_of_line_callback(int last_char, void * data) {
  struct rcsv_metadata * meta = (struct rcsv_metadata *) data;

  /* If filters didn't match, current row parsing is reverted */
  if (meta->skip_current_row) {
    /* Do we wanna GC? */
    meta->skip_current_row = false;
  } else {
    if (rb_block_given_p()) { /* STREAMING */
      rb_yield(meta->last_entry);
    } else {
      rb_ary_push(*(meta->result), meta->last_entry);
    }
  }

  /* Re-initialize last_entry unless EOF reached */
  if (last_char != -1) {
    if (meta->row_as_hash) {
      meta->last_entry = rb_hash_new(); /* {} */
    } else {
      meta->last_entry = rb_ary_new(); /* [] */
    }
  }

  /* Resetting column counter */
  meta->current_col = 0;

  /* Incrementing row counter */
  meta->current_row++;
  return;
}

/* Filter rows need to be either nil or Arrays of Strings/bools/nil, so we validate this here */
VALUE validate_filter_row(const char * filter_name, VALUE row) {
  if (row == Qnil) {
    return Qnil;
  } else if (TYPE(row) == T_ARRAY) {
    size_t j;
    for (j = 0; j < (size_t)RARRAY_LEN(row); j++) {
      switch (TYPE(rb_ary_entry(row, j))) {
        case T_NIL:
        case T_TRUE:
        case T_FALSE:
        case T_FLOAT:
        case T_FIXNUM:
        case T_BIGNUM:
        case T_REGEXP:
        case T_STRING:
          break;
        default:
          rb_raise(rcsv_parse_error,
            ":%s can only accept nil or Array consisting of String, boolean or nil elements, but %s was provided.",
            filter_name, RSTRING_PTR(rb_inspect(row)));
      }
    }
    return row;
  } else {
    rb_raise(rcsv_parse_error,
      ":%s can only accept nil or Array as an element, but %s was provided.",
      filter_name, RSTRING_PTR(rb_inspect(row)));
  }
}

/* All the possible free()'s should be listed here.
   This function should be invoked before returning the result to Ruby or raising an exception. */
void free_memory(struct csv_parser * cp, struct rcsv_metadata * meta) {
  if (meta->only_rows != NULL) {
    free(meta->only_rows);
  }

  if (meta->except_rows != NULL) {
    free(meta->except_rows);
  }

  if (meta->row_defaults != NULL) {
    free(meta->row_defaults);
  }

  if (meta->column_names != NULL) {
    free(meta->column_names);
  }

  if (cp != NULL) {
    csv_free(cp);
  }
}

/* An rb_rescue()-compatible free_memory() wrapper that unpacks C pointers from Ruby's Fixnums */
VALUE rcsv_free_memory(VALUE ensure_container) {
  free_memory(
    (struct csv_parser *)NUM2LONG(rb_ary_entry(ensure_container, 3)),
    (struct rcsv_metadata *)NUM2LONG(rb_ary_entry(ensure_container, 2))
  );

  return Qnil;
}

/* An rb_rescue()-compatible Ruby pseudo-method that handles the actual parsing */
VALUE rcsv_raw_parse(VALUE ensure_container) {
  /* Unpacking multiple variables from a single Ruby VALUE */
  VALUE options = rb_ary_entry(ensure_container, 0);
  VALUE csvio   = rb_ary_entry(ensure_container, 1);
  struct rcsv_metadata * meta = (struct rcsv_metadata *)NUM2LONG(rb_ary_entry(ensure_container, 2));
  struct csv_parser * cp = (struct csv_parser *)NUM2LONG(rb_ary_entry(ensure_container, 3));

  /* Helper temporary variables */
  VALUE option, csvstr, buffer_size;

  /* libcsv-related temporary variables */
  char * csv_string;
  size_t csv_string_len;
  int error;

  /* Generic iterator */
  size_t i = 0;

  /* IO buffer size can be controller via an option */
  buffer_size = rb_hash_aref(options, ID2SYM(rb_intern("buffer_size")));

  /* By default, parse as Array of Arrays */
  option = rb_hash_aref(options, ID2SYM(rb_intern("row_as_hash")));
  if (option && (option != Qnil)) {
    meta->row_as_hash = true;
  }

  /* :col_sep sets the column separator, default is comma (,) */
  option = rb_hash_aref(options, ID2SYM(rb_intern("col_sep")));
  if (option != Qnil) {
    csv_set_delim(cp, (unsigned char)*StringValuePtr(option));
  }

  /* Specify how many rows to skip from the beginning of CSV */
  option = rb_hash_aref(options, ID2SYM(rb_intern("offset_rows")));
  if (option != Qnil) {
    meta->offset_rows = (size_t)NUM2INT(option);
  }

  /* Specify the character encoding of the input data */
  option = rb_hash_aref(options, ID2SYM(rb_intern("output_encoding")));
  if (option && (option != Qnil)) {
    meta->encoding_index = RB_ENC_FIND_INDEX(StringValueCStr(option));
  }

  /* :only_rows is a list of values where row is only parsed
     if its fields match those in the passed array.
     [nil, nil, ["ABC", nil, 1]] skips all rows where 3rd column isn't equal to "ABC", nil or 1 */
  option = rb_hash_aref(options, ID2SYM(rb_intern("only_rows")));
  if (option != Qnil) {
    meta->num_only_rows = (size_t)RARRAY_LEN(option);
    meta->only_rows = (VALUE *)malloc(meta->num_only_rows * sizeof(VALUE));

    for (i = 0; i < meta->num_only_rows; i++) {
      VALUE only_row = rb_ary_entry(option, i);
      meta->only_rows[i] = validate_filter_row("only_rows", only_row);
    }
  }

  /* :except_rows is a list of values where row is only parsed
     if its fields don't match those in the passed array.
     [nil, nil, ["ABC", nil, 1]] skips all rows where 3rd column is equal to "ABC", nil or 1 */
  option = rb_hash_aref(options, ID2SYM(rb_intern("except_rows")));
  if (option != Qnil) {
    meta->num_except_rows = (size_t)RARRAY_LEN(option);
    meta->except_rows = (VALUE *)malloc(meta->num_except_rows * sizeof(VALUE));

    for (i = 0; i < meta->num_except_rows; i++) {
      VALUE except_row = rb_ary_entry(option, i);
      meta->except_rows[i] = validate_filter_row("except_rows", except_row);
    }
  }

  /* :row_defaults is an array of default values that are assigned to fields containing empty strings
     according to matching field positions */
  option = rb_hash_aref(options, ID2SYM(rb_intern("row_defaults")));
  if (option != Qnil) {
    meta->num_row_defaults = RARRAY_LEN(option);
    meta->row_defaults = (VALUE*)malloc(meta->num_row_defaults * sizeof(VALUE*));

    for (i = 0; i < meta->num_row_defaults; i++) {
      VALUE row_default = rb_ary_entry(option, i);
      meta->row_defaults[i] = row_default;
    }
  }

  /* :row_conversions specifies Ruby types that CSV field values should be converted into.
     Each char of row_conversions string represents Ruby type for CSV field with matching position. */
  option = rb_hash_aref(options, ID2SYM(rb_intern("row_conversions"))); 
  if (option != Qnil) {
    meta->num_row_conversions = RSTRING_LEN(option);
    meta->row_conversions = StringValuePtr(option);
  }

 /* Column names should be declared explicitly when parsing fields as Hashes */
  if (meta->row_as_hash) { /* Only matters for hash results */
    option = rb_hash_aref(options, ID2SYM(rb_intern("column_names"))); 
    if (option == Qnil) {
      rb_raise(rcsv_parse_error, ":row_as_hash requires :column_names to be set.");
    } else {
      meta->last_entry = rb_hash_new();

      meta->num_columns = (size_t)RARRAY_LEN(option);
      meta->column_names = (VALUE*)malloc(meta->num_columns * sizeof(VALUE*));

      for (i = 0; i < meta->num_columns; i++) {
        meta->column_names[i] = rb_ary_entry(option, i);
      }
    }
  } else {
    meta->last_entry = rb_ary_new();
  }

  while(true) {
    csvstr = rb_funcall(csvio, rb_intern("read"), 1, buffer_size);
    if ((csvstr == Qnil) || (RSTRING_LEN(csvstr) == 0)) { break; }

    csv_string = StringValuePtr(csvstr);
    csv_string_len = strlen(csv_string);

    /* Actual parsing and error handling */
    if (csv_string_len != csv_parse(cp, csv_string, csv_string_len,
                                    &end_of_field_callback, &end_of_line_callback, meta)) {
      error = csv_error(cp);
      switch(error) {
        case CSV_EPARSE:
          rb_raise(rcsv_parse_error, "Error when parsing malformed data");
          break;
        case CSV_ENOMEM:
          rb_raise(rcsv_parse_error, "No memory");
          break;
        case CSV_ETOOBIG:
          rb_raise(rcsv_parse_error, "Field data is too large");
          break;
        case CSV_EINVALID:
          rb_raise(rcsv_parse_error, "%s", (const char *)csv_strerror(error));
        break;
        default:
          rb_raise(rcsv_parse_error, "Failed due to unknown reason");
      }
    }
  }

  /* Flushing libcsv's buffer */
  csv_fini(cp, &end_of_field_callback, &end_of_line_callback, meta);

  return Qnil;
}

/* C API */

/* The main method that handles parsing */
static VALUE rb_rcsv_raw_parse(int argc, VALUE * argv, VALUE self) {
  struct rcsv_metadata meta;
  VALUE csvio, options, option;
  VALUE ensure_container = rb_ary_new(); /* [] */

  struct csv_parser cp;
  unsigned char csv_options = CSV_STRICT_FINI | CSV_APPEND_NULL;

  /* Setting up some sane defaults */
  meta.row_as_hash = false;
  meta.empty_field_is_nil = false;
  meta.skip_current_row = false;
  meta.encoding_index = -1;
  meta.num_columns = 0;
  meta.current_col = 0;
  meta.current_row = 0;
  meta.offset_rows = 0;
  meta.num_only_rows = 0;
  meta.num_except_rows = 0;
  meta.num_row_defaults = 0;
  meta.num_row_conversions = 0;
  meta.only_rows = NULL;
  meta.except_rows = NULL;
  meta.row_defaults = NULL;
  meta.row_conversions = NULL;
  meta.column_names = NULL;
  meta.result = (VALUE[]){rb_ary_new()}; /* [] */

  /* csvio is required, options is optional (pun intended) */
  rb_scan_args(argc, argv, "11", &csvio, &options);

  /* options ||= {} */
  if (NIL_P(options)) {
    options = rb_hash_new();
  }

  /* First of all, we parse libcsv-related params so that it fails early if something is wrong with them */

  /* By default, parsing is strict */
  option = rb_hash_aref(options, ID2SYM(rb_intern("nostrict")));
  if (!option || (option == Qnil)) {
    csv_options |= CSV_STRICT;
  }

  /* By default, empty strings are treated as Nils and quoted empty strings are treated as empty Ruby strings */
  option = rb_hash_aref(options, ID2SYM(rb_intern("parse_empty_fields_as")));
  if ((option == Qnil) || (option == ID2SYM(rb_intern("nil_or_string")))) {
    csv_options |= CSV_EMPTY_IS_NULL;
  } else if (option == ID2SYM(rb_intern("nil"))) {
    meta.empty_field_is_nil = true;
  } else if (option == ID2SYM(rb_intern("string"))) {
    meta.empty_field_is_nil = false;
  } else {
    rb_raise(rcsv_parse_error, "The only valid options for :parse_empty_fields_as are :nil, :string and :nil_or_string, but %s was supplied.", RSTRING_PTR(rb_inspect(option)));
  }

  /* rb_ensure() only expects callback functions to accept and return VALUEs */
  /* This ugly hack converts C pointers into Ruby Fixnums in order to pass them in Array */
  rb_ary_push(ensure_container, options);               /* [options] */
  rb_ary_push(ensure_container, csvio);                 /* [options, csvio] */
  rb_ary_push(ensure_container, LONG2NUM((long)&meta)); /* [options, csvio, &meta] */
  rb_ary_push(ensure_container, LONG2NUM((long)&cp));   /* [options, csvio, &meta, &cp] */

  /* Try to initialize libcsv */
  if (csv_init(&cp, csv_options) == -1) {
    rb_raise(rcsv_parse_error, "Couldn't initialize libcsv");
  }

  /* From now on, cp handles allocated data and should be free'd on exit or exception */
  rb_ensure(rcsv_raw_parse, ensure_container, rcsv_free_memory, ensure_container);

  /* Remove the last row if it's empty. That happens if CSV file ends with a newline. */
  if (RARRAY_LEN(*(meta.result)) && /* meta.result.size != 0 */
      RARRAY_LEN(rb_ary_entry(*(meta.result), -1)) == 0) {
    rb_ary_pop(*(meta.result));
  }

  if (rb_block_given_p()) {
    return Qnil; /* STREAMING */
  } else {
    return *(meta.result); /* Return accumulated result */
  }
}

/* Define Ruby API */
void Init_rcsv(void) {
  VALUE klass = rb_define_class("Rcsv", rb_cObject); /* class Rcsv; end */

  /* Error is initialized through static variable in order to access it from rb_rcsv_raw_parse */
  rcsv_parse_error = rb_define_class_under(klass, "ParseError", rb_eStandardError);

  /* def Rcsv.raw_parse; ...; end */
  rb_define_singleton_method(klass, "raw_parse", rb_rcsv_raw_parse, -1);
}
