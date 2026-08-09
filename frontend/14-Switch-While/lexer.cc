#include "lexer.h"

llvm::StringRef Token::GetSpellingText(TokenType tokenType)
{
    switch (tokenType)
    {
    case TokenType::kw_int:
        return "int";
    case TokenType::kw_if:
        return "if";
    case TokenType::kw_else:
        return "else";
    case TokenType::kw_for:
        return "for";
    case TokenType::kw_break:
        return "break";
    case TokenType::kw_continue:
        return "continue";
    case TokenType::kw_sizeof:
        return "sizeof";
    case TokenType::minus:
        return "-";
    case TokenType::plus:
        return "+";
    case TokenType::star:
        return "*";
    case TokenType::div:
        return "/";
    case TokenType::l_parent:
        return "(";
    case TokenType::r_parent:
        return ")";
    case TokenType::semi:
        return ";";
    case TokenType::equal:
        return "=";
    case TokenType::comma:
        return ",";
    case TokenType::number:
        return "number";
    case TokenType::identifier:
        return "identifier";
    case TokenType::l_brace:
        return "{";
    case TokenType::r_brace:
        return "}";
    case TokenType::equal_equal:
        return "==";
    case TokenType::not_equal:
        return "!=";
    case TokenType::less:
        return "<";
    case TokenType::less_equal:
        return "<=";
    case TokenType::greater:
        return ">";
    case TokenType::greater_equal:
        return ">=";
    case TokenType::pipepipe:
        return "||";
    case TokenType::pipe:
        return "|";
    case TokenType::ampamp:
        return "&&";
    case TokenType::amp:
        return "&";
    case TokenType::percent:
        return "%";
    case TokenType::less_less:
        return "<<";
    case TokenType::greater_greater:
        return ">>";
    case TokenType::caret:
        return "^";
    case TokenType::plus_plus:
        return "++";
    case TokenType::minus_minus:
        return "--";
    case TokenType::tilde:
        return "~";
    case TokenType::exclaim:
        return "!";
    case TokenType::plus_equal:
        return "+=";
    case TokenType::minus_equal:
        return "-=";
    case TokenType::star_equal:
        return "*=";
    case TokenType::slash_equal:
        return "/=";
    case TokenType::percent_equal:
        return "%=";
    case TokenType::less_less_equal:
        return "<<=";
    case TokenType::greater_greater_equal:
        return ">>=";
    case TokenType::amp_equal:
        return "&=";
    case TokenType::caret_equal:
        return "^=";
    case TokenType::pipe_equal:
        return "|=";
    case TokenType::question:
        return "?";
    case TokenType::colon:
        return ":";
    case TokenType::l_bracket:
        return "[";
    case TokenType::r_bracket:
        return "]";
    case TokenType::kw_struct:
        return "struct";
    case TokenType::kw_union:
        return "union";
    case TokenType::dot:
        return ".";
    case TokenType::arrow:
        return "->";
    case TokenType::kw_return:
        return "return";
    case TokenType::kw_void:
        return "void";
    case TokenType::kw_char:
        return "char";
    case TokenType::string:
        return "string";
    case TokenType::kw_const:
        return "const";
    case TokenType::kw_extern:
        return "extern";
    case TokenType::kw_volatile:
        return "volatile";
    case TokenType::kw_static:
        return "static";
    case TokenType::kw_do:
        return "do";
    case TokenType::kw_while:
        return "while";
    case TokenType::kw_switch:
        return "switch";    
    case TokenType::kw_case:
        return "case";
    case TokenType::kw_default:
        return "default";                
    case TokenType::ellipse:
        return "...";                              
    default:
        llvm::llvm_unreachable_internal();
    }
}

bool IsWhiteSpace(char ch)
{
    return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
}

bool IsDigit(char ch)
{
    return (ch >= '0') && (ch <= '9');
}

bool IsHexDigit(char ch)
{
    return IsDigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

bool IsLetter(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch == '_');
}

static const char *c_char(int *res, const char *p)
{
    if (*p != '\\')
    {
        *res = *p;
        return p + 1;
    }
    p++;

    int esc = 0;
    switch (*p)
    {
    case 'a':
    {
        esc = '\a';
        break;
    }
    case 'b':
    {
        esc = '\b';
        break;
    }
    case 'f':
    {
        esc = '\f';
        break;
    }
    case 'n':
    {
        esc = '\n';
        break;
    }
    case 'r':
    {
        esc = '\r';
        break;
    }
    case 't':
    {
        esc = '\t';
        break;
    }
    case 'v':
    {
        esc = '\v';
        break;
    }
    default:
    {
        esc = *p;
        break;
    }
    }

    *res = esc;
    return p + 1;
}

bool Lexer::StartWith(const char *p)
{
    return !strncmp(BufPtr, p, strlen(p));
}

void Lexer::NextToken(Token &tok)
{
    // 1. 过滤空格
    while (IsWhiteSpace(*BufPtr) || StartWith("//") || StartWith("/*"))
    {

        // 注释
        if (StartWith("//"))
        {
            while ((*BufPtr != '\n') && (BufPtr < BufEnd))
            {
                BufPtr++;
            }
        }

        if (StartWith("/*"))
        {
            while ((BufPtr[0] != '*' || BufPtr[1] != '/') && (BufPtr < BufEnd))
            {
                if (*BufPtr == '\n')
                {
                    row++;
                    LineHeadPtr = BufPtr + 1;
                }
                BufPtr++;
            }
            BufPtr += 2;
        }

        if (BufPtr >= BufEnd)
        {
            tok.tokenType = TokenType::eof;
            return;
        }

        if (*BufPtr == '\n')
        {
            row++;
            LineHeadPtr = BufPtr + 1;
        }
        BufPtr++;
    }

    // 2. 判断是否到结尾
    if (BufPtr >= BufEnd)
    {
        tok.tokenType = TokenType::eof;
        return;
    }

    tok.row = row;
    tok.col = BufPtr - LineHeadPtr + 1;

    const char *StartPtr = BufPtr;

    if (BufPtr[0] == '\'')
    {
        tok.tokenType = TokenType::number;
        BufPtr++; // skip '
        tok.ptr = BufPtr;
        tok.ty = CType::IntType;
        BufPtr = c_char(&tok.value, BufPtr);
        if (BufPtr[0] != '\'')
        {
            diagEngine.Report(llvm::SMLoc::getFromPointer(BufPtr), diag::err_unclosed_character);
        }
        tok.len = BufPtr - StartPtr;
        BufPtr++; // skip right '
    }
    else if (BufPtr[0] == '\"')
    {
        BufPtr++; // skip "
        tok.tokenType = TokenType::string;
        tok.ptr = BufPtr;
        std::string value;
        while (*BufPtr != '"')
        {
            if (!*BufPtr)
            {
                // \0
                diagEngine.Report(llvm::SMLoc::getFromPointer(BufPtr), diag::err_unclosed_string);
            }
            int c;
            BufPtr = c_char(&c, BufPtr);
            value += c;
        }
        tok.strValue = value;
        tok.ty = std::make_shared<CArrayType>(CType::CharType, value.length());
        tok.len = BufPtr - tok.ptr;
        BufPtr++;
    }
    else if (StartWith("0x") || StartWith("0X"))
    {
        BufPtr += 2;
        int number = 0;
        while (IsHexDigit(*BufPtr))
        {
            number = number * 16 + (*BufPtr - '0');
        }
        tok.tokenType = TokenType::number;
        tok.value = number;
        tok.ty = CType::IntType;
        tok.ptr = StartPtr;
        tok.len = BufPtr - StartPtr;
    }
    else if (IsDigit(*BufPtr))
    {
        int number = 0;
        while (IsDigit(*BufPtr))
        {
            number = number * 10 + (*BufPtr - '0');
            BufPtr++;
        }
        tok.tokenType = TokenType::number;
        tok.value = number;
        tok.ty = CType::IntType;
        tok.ptr = StartPtr;
        tok.len = BufPtr - StartPtr;
    }
    else if (IsLetter(*BufPtr))
    {
        while (IsLetter(*BufPtr) || IsDigit(*BufPtr))
        {
            BufPtr++;
        }
        tok.tokenType = TokenType::identifier;
        tok.ptr = StartPtr;
        tok.len = BufPtr - StartPtr;
        llvm::StringRef text = llvm::StringRef(tok.ptr, tok.len);
        if (text == "int")
        {
            tok.tokenType = TokenType::kw_int;
        }
        else if (text == "if")
        {
            tok.tokenType = TokenType::kw_if;
        }
        else if (text == "else")
        {
            tok.tokenType = TokenType::kw_else;
        }
        else if (text == "for")
        {
            tok.tokenType = TokenType::kw_for;
        }
        else if (text == "break")
        {
            tok.tokenType = TokenType::kw_break;
        }
        else if (text == "continue")
        {
            tok.tokenType = TokenType::kw_continue;
        }
        else if (text == "sizeof")
        {
            tok.tokenType = TokenType::kw_sizeof;
        }
        else if (text == "struct")
        {
            tok.tokenType = TokenType::kw_struct;
        }
        else if (text == "union")
        {
            tok.tokenType = TokenType::kw_union;
        }
        else if (text == "return")
        {
            tok.tokenType = TokenType::kw_return;
        }
        else if (text == "void")
        {
            tok.tokenType = TokenType::kw_void;
        }
        else if (text == "char")
        {
            tok.tokenType = TokenType::kw_char;
        }
        else if (text == "const")
        {
            tok.tokenType = TokenType::kw_const;
        }
        else if (text == "static")
        {
            tok.tokenType = TokenType::kw_static;
        }
        else if (text == "volatile")
        {
            tok.tokenType = TokenType::kw_volatile;
        }
        else if (text == "extern")
        {
            tok.tokenType = TokenType::kw_extern;
        }
        else if(text == "switch")
        {
            tok.tokenType = TokenType::kw_switch;
        }
        else if(text == "do")
        {
            tok.tokenType = TokenType::kw_do;
        }
        else if(text == "while")
        {
            tok.tokenType = TokenType::kw_while;
        }
        else if(text == "case")
        {
            tok.tokenType = TokenType::kw_case;
        }
        else if(text == "default")
        {
            tok.tokenType = TokenType::kw_default;
        }
    }
    else
    {
        switch (*BufPtr)
        {
        case '+':
            if (BufPtr[1] == '+')
            {
                tok.tokenType = TokenType::plus_plus;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else if (BufPtr[1] == '=')
            {
                tok.tokenType = TokenType::plus_equal;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else
            {
                tok.tokenType = TokenType::plus;
                BufPtr++;
                tok.ptr = StartPtr;
                tok.len = 1;
            }
            break;
        case '-':
            if (BufPtr[1] == '-')
            {
                tok.tokenType = TokenType::minus_minus;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else if (BufPtr[1] == '=')
            {
                tok.tokenType = TokenType::minus_equal;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else if (BufPtr[1] == '>')
            {
                tok.tokenType = TokenType::arrow;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else
            {
                tok.tokenType = TokenType::minus;
                BufPtr++;
                tok.ptr = StartPtr;
                tok.len = 1;
            }
            break;
        case '*':
            if (BufPtr[1] == '=')
            {
                tok.tokenType = TokenType::star_equal;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else
            {
                tok.tokenType = TokenType::star;
                BufPtr++;
                tok.ptr = StartPtr;
                tok.len = 1;
            }
            break;
        case '/':
            if (BufPtr[1] == '=')
            {
                tok.tokenType = TokenType::slash_equal;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else
            {
                tok.tokenType = TokenType::div;
                BufPtr++;
                tok.ptr = StartPtr;
                tok.len = 1;
            }
            break;
        case '%':
            if (BufPtr[1] == '=')
            {
                tok.tokenType = TokenType::percent_equal;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else
            {
                tok.tokenType = TokenType::percent;
                BufPtr++;
                tok.ptr = StartPtr;
                tok.len = 1;
            }
            break;
        case ';':
            tok.tokenType = TokenType::semi;
            BufPtr++;
            tok.ptr = StartPtr;
            tok.len = 1;
            break;
        case '(':
            tok.tokenType = TokenType::l_parent;
            BufPtr++;
            tok.ptr = StartPtr;
            tok.len = 1;
            break;
        case ')':
            tok.tokenType = TokenType::r_parent;
            BufPtr++;
            tok.ptr = StartPtr;
            tok.len = 1;
            break;
        case '^':
            if (BufPtr[1] == '=')
            {
                tok.tokenType = TokenType::caret_equal;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else
            {
                tok.tokenType = TokenType::caret;
                BufPtr++;
                tok.ptr = StartPtr;
                tok.len = 1;
            }
            break;
        case '=':
            if (BufPtr[1] == '=')
            {
                tok.tokenType = TokenType::equal_equal;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else
            {
                tok.tokenType = TokenType::equal;
                BufPtr++;
                tok.ptr = StartPtr;
                tok.len = 1;
            }
            break;
        case '&':
            if (BufPtr[1] == '&')
            {
                tok.tokenType = TokenType::ampamp;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else if (BufPtr[1] == '=')
            {
                tok.tokenType = TokenType::amp_equal;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else
            {
                tok.tokenType = TokenType::amp;
                BufPtr++;
                tok.ptr = StartPtr;
                tok.len = 1;
            }
            break;
        case '|':
            if (BufPtr[1] == '|')
            {
                tok.tokenType = TokenType::pipepipe;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else if (BufPtr[1] == '=')
            {
                tok.tokenType = TokenType::pipe_equal;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else
            {
                tok.tokenType = TokenType::pipe;
                BufPtr++;
                tok.ptr = StartPtr;
                tok.len = 1;
            }
            break;
        case ',':
            tok.tokenType = TokenType::comma;
            BufPtr++;
            tok.ptr = StartPtr;
            tok.len = 1;
            break;
        case '{':
            tok.tokenType = TokenType::l_brace;
            BufPtr++;
            tok.ptr = StartPtr;
            tok.len = 1;
            break;
        case '}':
            tok.tokenType = TokenType::r_brace;
            BufPtr++;
            tok.ptr = StartPtr;
            tok.len = 1;
            break;
        case '[':
            tok.tokenType = TokenType::l_bracket;
            BufPtr++;
            tok.ptr = StartPtr;
            tok.len = 1;
            break;
        case ']':
            tok.tokenType = TokenType::r_bracket;
            BufPtr++;
            tok.ptr = StartPtr;
            tok.len = 1;
            break;
        case '~':
            tok.tokenType = TokenType::tilde;
            BufPtr++;
            tok.ptr = StartPtr;
            tok.len = 1;
            break;
        case ':':
            tok.tokenType = TokenType::colon;
            BufPtr++;
            tok.ptr = StartPtr;
            tok.len = 1;
            break;
        case '?':
            tok.tokenType = TokenType::question;
            BufPtr++;
            tok.ptr = StartPtr;
            tok.len = 1;
            break;
        case '.':
            if (BufPtr[1] == '.' && BufPtr[2] == '.')
            {
                tok.tokenType = TokenType::ellipse;
                BufPtr += 3;
                tok.ptr = StartPtr;
                tok.len = 3;
            }
            else
            {
                tok.tokenType = TokenType::dot;
                BufPtr++;
                tok.ptr = StartPtr;
                tok.len = 1;
            }
            break;
        case '!':
            if (*(BufPtr + 1) == '=')
            {
                tok.tokenType = TokenType::not_equal;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else
            {
                tok.tokenType = TokenType::exclaim;
                BufPtr += 1;
                tok.ptr = StartPtr;
                tok.len = 1;
            }
            break;
        case '<':
            if (BufPtr[1] == '=')
            {
                tok.tokenType = TokenType::less_equal;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else if (BufPtr[1] == '<')
            {
                if (BufPtr[2] == '=')
                {
                    tok.tokenType = TokenType::less_less_equal;
                    BufPtr += 3;
                    tok.ptr = StartPtr;
                    tok.len = 3;
                }
                else
                {
                    tok.tokenType = TokenType::less_less;
                    BufPtr += 2;
                    tok.ptr = StartPtr;
                    tok.len = 2;
                }
            }
            else
            {
                tok.tokenType = TokenType::less;
                BufPtr++;
                tok.ptr = StartPtr;
                tok.len = 1;
            }
            break;
        case '>':
            if (*(BufPtr + 1) == '=')
            {
                tok.tokenType = TokenType::greater_equal;
                BufPtr += 2;
                tok.ptr = StartPtr;
                tok.len = 2;
            }
            else if (*(BufPtr + 1) == '>')
            {
                if (BufPtr[2] == '=')
                {
                    tok.tokenType = TokenType::greater_greater_equal;
                    BufPtr += 3;
                    tok.ptr = StartPtr;
                    tok.len = 3;
                }
                else
                {
                    tok.tokenType = TokenType::greater_greater;
                    BufPtr += 2;
                    tok.ptr = StartPtr;
                    tok.len = 2;
                }
            }
            else
            {
                tok.tokenType = TokenType::greater;
                BufPtr++;
                tok.ptr = StartPtr;
                tok.len = 1;
            }
            break;
        default:
            diagEngine.Report(llvm::SMLoc::getFromPointer(BufPtr), diag::err_unknown_char, *BufPtr);
            break;
        }
    }
}

void Lexer::SaveState()
{
    state.BufPtr = BufPtr;
    state.BufEnd = BufEnd;
    state.LineHeadPtr = LineHeadPtr;
    state.row = row;
}

void Lexer::RestoreState()
{
    BufPtr = state.BufPtr;
    BufEnd = state.BufEnd;
    LineHeadPtr = state.LineHeadPtr;
    row = state.row;
}