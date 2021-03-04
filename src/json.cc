#include "co/json.h"
#include "co/str.h"

namespace json {

Value::Jalloc::~Jalloc() {
    for (uint32 i = 0; i < _mp.size(); ++i) free((void*)_mp[i]);
    for (uint32 i = 0; i < _ks[0].size(); ++i) free((char*)_ks[0][i] - 8);
    for (uint32 i = 0; i < _ks[1].size(); ++i) free((char*)_ks[1][i] - 8);
    for (uint32 i = 0; i < _ks[2].size(); ++i) free((char*)_ks[2][i] - 8);
}

void* Value::Jalloc::alloc(uint32 n) {
    char* p;
    if (n <= 24) {
        if (!_ks[0].empty()) return (void*) _ks[0].pop_back();
        p = (char*) malloc(32);
        *p = 0;
    } else if (n <= 56) {
        if (!_ks[1].empty()) return (void*) _ks[1].pop_back();
        p = (char*) malloc(64);
        *p = 1;
    } else if (n <= 120) {
        if (!_ks[2].empty()) return (void*) _ks[2].pop_back();
        p = (char*) malloc(128);
        *p = 2;
    } else {
        p = (char*) malloc(n + 8);
        *p = 3;
    }

    return p + 8;
}

void Value::Jalloc::dealloc(void* p) {
    char* s = (char*)p - 8;
    int c = *s; // 0, 1, 2, 3
    if (c < 3 && _ks[c].size() < 4095) {
        _ks[c].push_back(p);
    } else {
        free(s);
    }
}

Value& Value::operator[](Key key) const {
    if (_mem) assert(_mem->type & kObject);
    else new ((void*)&_mem) Value(Value::JObject());

    for (auto it = this->begin(); it != this->end(); ++it) {
        if (strcmp(it->key, key) == 0) return it->value;
    }

    _mem->a.push_back(_Alloc_key(key));
    _mem->a.push_back(0); // null value 
    return *(Value*) &_mem->a.back();
}

Value Value::find(Key key) const {
    if (this->is_object()) {
        for (auto it = this->begin(); it != this->end(); ++it) {
            if (strcmp(it->key, key) == 0) return it->value;
        }
    }
    return Value();
}

bool Value::has_member(Key key) const {
    if (this->is_object()) {
        for (auto it = this->begin(); it != this->end(); ++it) {
            if (strcmp(it->key, key) == 0) return true;
        }
    }
    return false;
}

void Value::_UnRef() {
    if (atomic_dec(&_mem->refn) == 0) {
        if (_mem->type & kObject) {
            Array& a = _mem->a;
            for (uint32 i = 0; i < a.size(); i += 2) {
                Jalloc::instance()->dealloc((void*)a[i]);
                ((Value*)&a[i + 1])->~Value();
            }
            a.~Array();
        } else if (_mem->type & kArray) {
            Array& a = _mem->a;
            for (uint32 i = 0; i < a.size(); ++i) {
                ((Value*)&a[i])->~Value();
            }
            a.~Array();
        } else if (_mem->type & kString) {
            Jalloc::instance()->dealloc(_mem->s);
        }
        Jalloc::instance()->dealloc_mem(_mem);
    }
}

static inline const char* init_e2s_table() {
    static char tb[256] = { 0 };
    tb['\r'] = 'r';
    tb['\n'] = 'n';
    tb['\t'] = 't';
    tb['\b'] = 'b';
    tb['\f'] = 'f';
    tb['\"'] = '"';
    tb['\\'] = '\\';
    return tb;
}

void Value::_Json2str(fastream& fs, bool debug) const {
    if (_mem == 0) {
        fs << "null";

    } else if (_mem->type & kString) {
        fs << '"';
        static const char* tb = init_e2s_table();
        uint32 len = _mem->l[-1];
        bool trunc = debug && len > 256;
        const char* s = _mem->s;
        const char* e = trunc ? s + 256 : s + len;

        for (const char* p = s; p != e; ++p) {
            char c = tb[(uint8)*p];
            if (c) {
                fs.append(s, p - s).append('\\').append(c);
                s = p + 1;
            }
        }

        if (s != e) fs.append(s, e - s);
        if (trunc) fs.append(3, '.');
        fs << '"';

    } else if (_mem->type & kObject) {
        fs << '{';
        auto it = this->begin();
        if (it != this->end()) {
            fs << '"' << it->key << '"' << ':';
            it->value._Json2str(fs, debug);
            for (; ++it != this->end();) {
                fs << ',' << '"' << it->key << '"' << ':';
                it->value._Json2str(fs, debug);
            }
        }
        fs << '}';

    } else if (_mem->type & kArray) {
        fs << '[';
        auto& a = _mem->a;
        if (!a.empty()) {
            ((Value*)&a[0])->_Json2str(fs, debug);
            for (uint32 i = 1; i < a.size(); ++i) {
                fs << ',';
                ((Value*)&a[i])->_Json2str(fs, debug);
            }
        }
        fs << ']';

    } else {
        if (_mem->type & kInt) {
            fs << _mem->i;
        } else if (_mem->type & kBool) {
            fs << _mem->b;
        } else {
            assert(_mem->type & kDouble);
            fs << _mem->d;
        }
    }
}

// @indent:  4 spaces by default
// @n:       number of spaces to insert at the beginning for the current line
void Value::_Json2pretty(fastream& fs, int indent, int n) const {
    if (_mem == 0) {
        fs << "null";

    } else if (_mem->type & kObject) {
        fs << '{';
        auto it = this->begin();
        if (it != this->end()) {
            for (;;) {
                fs.append('\n').append(n, ' ');
                fs << '"' << it->key << '"' << ": ";

                Value& v = it->value;
                if (v.is_object() || v.is_array()) {
                    v._Json2pretty(fs, indent, n + indent);
                } else {
                    v._Json2str(fs);
                }

                if (++it != this->end()) {
                    fs << ',';
                } else {
                    fs << '\n';
                    break;
                }
            }
        }

        if (n > indent) fs.append(n - indent, ' ');
        fs << '}';

    } else if (_mem->type & kArray) {
        fs << '[';
        auto& a = _mem->a;
        if (!a.empty()) {
            for (uint32 i = 0;;) {
                fs.append('\n').append(n, ' ');

                Value& v = *(Value*) &a[i];
                if (v.is_object() || v.is_array()) {
                    v._Json2pretty(fs, indent, n + indent);
                } else {
                    v._Json2str(fs);
                }

                if (++i < a.size()) {
                    fs << ',';
                } else {
                    fs << '\n';
                    break;
                }
            }
        }

        if (n > indent) fs.append(n - indent, ' ');
        fs << ']';

    } else {
        _Json2str(fs);
    }
}

// json parser
//   @b: beginning of the string
//   @e: end of the string
//   @s: stack for parsing string type
//   @r: result
// return the current position, or NULL on any error
const char* parse_object(const char* b, const char* e, fastream& s, Value* r);
const char* parse_array(const char* b, const char* e, fastream& s, Value* r);
const char* parse_string(const char* b, const char* e, fastream& s, Value* r);
const char* parse_unicode(const char* b, const char* e, fastream& s);
const char* parse_number(const char* b, const char* e, Value* r);

inline bool is_white_char(char c) {
    return (c == ' ' || c == '\n' || c == '\r' || c == '\t');
}

inline const char* parse_key(const char* b, const char* e, char** key) {
    if (*b++ != '"') return 0;
    const char* p = (const char*) memchr(b, '"', e - b);
    if (p) {
        char* s = (char*) Json::Jalloc::instance()->alloc((uint32)(p - b + 1));
        memcpy(s, b, p - b);
        s[p - b] = '\0';
        *key = s;
    }
    return p;
}

inline const char* parse_false(const char* b, const char* e, Value* r) {
    if (e - b < 5) return 0;
    if (b[1] != 'a' || b[2] != 'l' || b[3] != 's' || b[4] != 'e') return 0;
    new (r) Value(false);
    return b + 4;
}

inline const char* parse_true(const char* b, const char* e, Value* r) {
    if (e - b < 4) return 0;
    if (b[1] != 'r' || b[2] != 'u' || b[3] != 'e') return 0;
    new (r) Value(true);
    return b + 3;
}

inline const char* parse_null(const char* b, const char* e, Value* r) {
    if (e - b < 4) return 0;
    if (b[1] != 'u' || b[2] != 'l' || b[3] != 'l') return 0;
    return b + 3;
}

inline const char* parse_value(const char* b, const char* e, fastream& s, Value* r) {
    if (*b == '"') {
        return parse_string(b, e, s, r);
    } else if (*b == '{') {
        return parse_object(b, e, s, new (r) Value(Value::JObject()));
    } else if (*b == '[') {
        return parse_array(b, e, s, new (r) Value(Value::JArray()));
    } else if (*b == 'f') {
        return parse_false(b, e, r);
    } else if (*b == 't') {
        return parse_true(b, e, r);
    } else if (*b == 'n') {
        return parse_null(b, e, r);
    } else {
        return parse_number(b, e, r);
    }
}

static inline bool parse(const char* b, const char* e, Value* r) {
    while (b < e && is_white_char(*b)) ++b;
    if (b == e) return false;

    fastream s;
    if (*b == '{') {
        b = parse_object(b, e, s, new (r) Value(Value::JObject()));
    } else if (*b == '[') {
        b = parse_array(b, e, s, new (r) Value(Value::JArray()));
    } else {
        b = parse_value(b, e, s, r);
    }

    if (b == 0) return false;
    while (++b < e && is_white_char(*b));
    return b == e;
}

const char* parse_object(const char* b, const char* e, fastream& s, Value* r) {
    char* key;
    void* val;

    while (true) {
        while (++b < e && is_white_char(*b));
        if (b == e) return 0;
        if (*b == '}') return b; // object end

        // key
        b = parse_key(b, e, &key);
        if (b == 0) return 0;

        // ':'
        while (++b < e && is_white_char(*b));
        if (b == e || *b != ':') {
            Value::Jalloc::instance()->dealloc(key);
            return 0;
        }

        while (++b < e && is_white_char(*b));
        if (b == e) {
            Value::Jalloc::instance()->dealloc(key);
            return 0;
        }

        // value
        val = 0;
        b = parse_value(b, e, s, (Value*)&val);
        if (b == 0) {
            if (val != 0) ((Value*)&val)->~Value();
            Value::Jalloc::instance()->dealloc(key);
            return 0;
        }

        r->_mem->a.push_back(key);
        r->_mem->a.push_back(val);

        // check value end
        while (++b < e && is_white_char(*b));
        if (b == e) return 0;
        if (*b == '}') return b;
        if (*b != ',') return 0;
    }
}

const char* parse_array(const char* b, const char* e, fastream& s, Value* r) {
    while (true) {
        while (++b < e && is_white_char(*b));
        if (*b == ']') return b; // array end

        void* v = 0;
        b = parse_value(b, e, s, (Value*)&v);
        if (b == 0) {
            if (v) ((Value*)&v)->~Value();
            return 0;
        }

        r->_mem->a.push_back(v);

        while (++b < e && is_white_char(*b));
        if (b == e) return 0;
        if (*b == ']') return b; // array end
        if (*b != ',') return 0;
    }
}

// find '"' or '\\'
inline const char* find_quote_or_escape(const char* b, const char* e) {
    for (; b < e; ++b) {
        if (*b == '"' || *b == '\\') return b;
    }
    return 0;
}

static inline const char* init_s2e_table() {
    static char tb[256] = { 0 };
    tb['r'] = '\r';
    tb['n'] = '\n';
    tb['t'] = '\t';
    tb['b'] = '\b';
    tb['f'] = '\f';
    tb['"'] = '"';
    tb['\\'] = '\\';
    tb['/'] = '/';
    tb['u'] = 'u';
    return tb;
}

const char* parse_string(const char* b, const char* e, fastream& s, Value* r) {
    const char* p = find_quote_or_escape(++b, e); // find the first '"' or '\\'
    if (p == 0) return 0;
    if (*p == '"') {
        new (r) Value(b, p - b);
        return p;
    }

    s.clear();
    do {
        s.append(b, p - b);
        if (++p == e) return 0;

        static const char* tb = init_s2e_table();
        char c = tb[(uint8)*p];
        if (c == 0) return 0; // invalid escape

        if (*p != 'u') {
            s.append(c);
        } else {
            p = parse_unicode(p + 1, e, s);
            if (p == 0) return 0;
        }

        b = p + 1;
        p = find_quote_or_escape(b, e);
        if (p == 0) return 0;

        if (*p == '"') {
            s.append(b, p - b);
            new (r) Value(s.data(), s.size());
            return p;
        }
    } while (true);
}

inline const char* parse_hex(const char* b, const char* e, uint32& u) {
    if (e - b < 4) return 0;
    for (int i = 0; i < 4; ++i) {
        u <<= 4;
        char c = b[i];
        if ('0' <= c && c <= '9') {
            u += c - '0';
        } else if ('A' <= c && c <= 'F') {
            u += c - 'A' + 10;
        } else if ('a' <= c && c <= 'f') {
            u += c - 'a' + 10;
        } else {
            return 0;
        }
    }
    return b + 3;
}

// utf8:
//   0000 - 007F      0xxxxxxx            
//   0080 - 07FF      110xxxxx  10xxxxxx        
//   0800 - FFFF      1110xxxx  10xxxxxx  10xxxxxx    
//  10000 - 10FFFF    11110xxx  10xxxxxx  10xxxxxx  10xxxxxx
//
// \uXXXX
// \uXXXX\uYYYY
//   D800 <= XXXX <= DBFF
//   DC00 <= XXXX <= DFFF
const char* parse_unicode(const char* b, const char* e, fastream& s) {
    uint32 u = 0;
    b = parse_hex(b, e, u);
    if (b == 0) return 0;

    if (0xD800 <= u && u <= 0xDBFF) {
        if (e - b < 3) return 0;
        if (b[1] != '\\' || b[2] != 'u') return 0;

        uint32 v = 0;
        b = parse_hex(b + 3, e, v);
        if (b == 0) return 0;
        if (v < 0xDC00 || v > 0xDFFF) return 0;

        u = 0x10000 + (((u - 0xD800) << 10) | (v - 0xDC00));
    }

    // encode to UTF8
    if (u <= 0x7F) {
        s.append((char)u);
    } else if (u <= 0x7FF) {
        s.append((char) (0xC0 | (0xFF & (u >> 6))));
        s.append((char) (0x80 | (0x3F & u)));
    } else if (u <= 0xFFFF) {
        s.append((char) (0xE0 | (0xFF & (u >> 12))));
        s.append((char) (0x80 | (0x3F & (u >> 6))));
        s.append((char) (0x80 | (0x3F & u)));
    } else {
        assert(u <= 0x10FFFF);
        s.append((char) (0xF0 | (0xFF & (u >> 18))));
        s.append((char) (0x80 | (0x3F & (u >> 12))));
        s.append((char) (0x80 | (0x3F & (u >>  6))));
        s.append((char) (0x80 | (0x3F & u)));
    } 

    return b;
}

static int64 fastatoi(const char* s, size_t n) {
    uint64 v = 0;
    uint64 max_value;
    size_t max_digit;

    if (*s != '-') {
        max_value = MAX_UINT64;
        max_digit = 20;
    } else {
        max_value = (uint64)MIN_INT64;
        max_digit = 19;
        ++s;
        if (--n == 0) throw "invalid integer value";
    }

    if (*s == '0') {
        if (n == 1) return 0;
        throw "invalid integer value";
    }

    if (n > max_digit) throw "out of range for integer";

    for (size_t i = 0; i < n - 1; ++i) {
        if (s[i] < '0' || s[i] > '9') throw "invalid integer value";
        v = v * 10 + s[i] - '0';
    }

    char c = s[n - 1];
    if (c < '0' || c > '9') throw "invalid integer value";
    if (n == max_digit) {
        if (v > (max_value - (c - '0')) / 10) throw "out of range for integer";
    }

    v = v * 10 + c - '0';
    return max_digit == 20 ? v : -(int64)v;
}

const char* parse_number(const char* b, const char* e, Value* r) {
    bool is_double = false;
    const char* p = b;

    for (; p < e; ++p) {
        char c = *p;
        if (c == ',' || c == '}' || c == ']' || is_white_char(c)) break;
        if (c == '.' || c == 'e' || c == 'E') is_double = true;
    }

    if (p == b) return 0;
    try {
        if (!is_double) {
            new (r) Value(fastatoi(b, p - b));
        } else {
            new (r) Value(str::to_double(fastring(b, p - b)));
        }
    } catch (...) {
        return 0; // invalid number
    }
    
    return p - 1; // return pointer to the end of the number
}

bool Value::parse_from(const char* s, size_t n) {
    if (_mem) {
        this->_UnRef();
        _mem = 0;
    }
    return parse(s, s + n, this);
}

} // namespace json
