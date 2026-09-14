// =========================================================
// ファイルの情報[DialogueTextCodec.h]
//
// 制作者:Masatora Tanaka        日付：2026/09/13
// =========================================================
#pragma once

#include <string>

namespace story::dialogueTextCodec {

// =========================================================
// 会話ファイル内のエスケープ文字を表示用テキストへ戻す
// =========================================================
inline std::string decode(const std::string& value) {
    std::string decoded;
    decoded.reserve(value.size());
    for(size_t i=0;i<value.size();++i){
        if(value[i]=='\\'&&i+1<value.size()){
            if(value[i+1]=='n'){decoded+='\n';++i;continue;}
            if(value[i+1]=='\\'){decoded+='\\';++i;continue;}
        }
        decoded+=value[i];
    }
    return decoded;
}

// =========================================================
// 改行を一行形式の会話ファイルへ安全に保存する
// =========================================================
inline std::string encode(const std::string& value) {
    std::string encoded;
    encoded.reserve(value.size());
    for(const char character:value){
        if(character=='\\')encoded+="\\\\";
        else if(character=='\n')encoded+="\\n";
        else if(character!='\r')encoded+=character;
    }
    return encoded;
}

// =========================================================
// ヒロインの発話へ鉤括弧を補う
// =========================================================
inline std::string quoteHeroineSpeech(const std::string& speaker,const std::string& text) {
    const bool heroine=speaker=="少女"||speaker=="凪沙"||speaker=="ヒロイン";
    if(!heroine||(text.starts_with("「")&&text.ends_with("」")))return text;
    return "「"+text+"」";
}

}
