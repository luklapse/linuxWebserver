/*
 * @Author       : mark
 * @Date         : 2020-06-26
 * @copyleft Apache 2.0
 */
#include "httprequest.h"
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
    "/index",
    "/register",
    "/login",
    "/welcome",
    "/video",
    "/picture",
};

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG{
    {"/register.html", 0},
    {"/login.html", 1},
};

void HttpRequest::RequestClear()
{
    isKeepAlive_=false;
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::ParseRequest(Buff &buff)
{
    if(REQUEST_LINE != state_) return ParseHeader_(buff);
    static regex patten("^([^ \r\n]*) ([^ \r\n]*) HTTP/([^ \r\n]*)\r\n");
    cmatch subMatch;
    if (regex_search(buff.Peek(), subMatch, patten))
    {
        buff.PeekAdd(subMatch.length());
        method_ = subMatch[1].str();
        path_ = subMatch[2].str();
        version_ = subMatch[3].str();
        state_ = HEADERS;
        if (path_ == "/")
        {
            path_ = "/index.html";
        }
        else
        {   auto tmp=DEFAULT_HTML.find(path_);
            if(tmp!=DEFAULT_HTML.end()){
                path_ += ".html";
            }
        }
        return ParseHeader_(buff);
    }
    //LOG_ERROR("RequestLine Error");
    return false;
}

bool HttpRequest::ParseHeader_(Buff &buff)
{
    static regex patten("^([^:\r\n]*): ?([^\r\n]*)\r\n");
    cmatch subMatch;
    while (regex_search(buff.Peek(), subMatch, patten)) //
    {
        buff.PeekAdd(subMatch.length());
        header_[subMatch[1].str()] = subMatch[2].str();
    }
    return ParseBody_(buff);
}

bool HttpRequest::ParseBody_(Buff &buff)
{
    static regex patten("^([^\r\n]*)\r\n");
    cmatch subMatch;
    if (regex_search(buff.Peek(), subMatch, patten))
    {
        buff.PeekAdd(subMatch.length());
        body_ = subMatch[1].str();
    }
    else{
        return false;
    }
    if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded")
    {
        ParseFromUrlencoded_();
        if (DEFAULT_HTML_TAG.count(path_))
        {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if (tag == 0 || tag == 1)
            {
                bool isLogin = (tag == 1);
                if (UserVerify(post_["username"], post_["password"], isLogin))
                {
                    path_ = "/welcome.html";
                }
                else
                {
                    path_ = "/error.html";
                }
            }
        }
    }
    state_ = FINISH;
    isKeepAlive_=header_["Connection"]== "keep-alive"&&version_ == "1.1";
    LOG_DEBUG("Body:%s, len:%d", body_.c_str(), body_.size());
    return true;
}

void HttpRequest::ParseFromUrlencoded_()
{
    if (body_.size() == 0)
    {
        return;
    }

    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for (; i < n; i++)
    {
        char ch = body_[i];
        switch (ch)
        {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';
            break;
        case '%':
            num=stoi(body_.substr(i+1,2),nullptr,16);
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    if (post_.count(key) == 0 && j < i)
    {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin)
{
    if (name == "" || pwd == "")
    {
        return false;
    }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL *sql;
    SqlConnRAII(&sql, SqlConnPool::Instance());
    assert(sql);

    bool flag = false;
    unsigned int j = 0;
    char order[256] = {0};
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;

    if (!isLogin)
    {
        flag = true;
    }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if (mysql_query(sql, order))
    {
        mysql_free_result(res);
        return false;
    }
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    while (MYSQL_ROW row = mysql_fetch_row(res))
    {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        /* 注册行为 且 用户名未被使用*/
        if (isLogin)
        {
            if (pwd == password)
            {
                flag = true;
            }
            else
            {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        }
        else
        {
            flag = false;
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if (!isLogin && flag == true)
    {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        if (mysql_query(sql, order))
        {
            LOG_DEBUG("Insert error!");
            flag = false;
        }
        flag = true;
    }
    SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG("UserVerify success!!");
    return flag;
}
