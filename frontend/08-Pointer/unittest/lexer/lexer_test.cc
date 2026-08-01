#include <gtest/gtest.h>
#include "lexer.h"


class LexerTest : public ::testing::Test
{
public:
    void SetUp() override {
        /// TESTSET_DIR 由 CMake 注入的绝对路径，跟当前工作目录无关
        llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> buf =
            llvm::MemoryBuffer::getFile(TESTSET_DIR "/lexer_01.txt");
        ASSERT_TRUE(buf) << "can't open file: " << TESTSET_DIR "/lexer_01.txt";

        /// mgr / diagEngine 必须是成员: Lexer 持有的是它们的引用，
        /// 放在栈上出了 SetUp 就悬空了
        mgr.AddNewSourceBuffer(std::move(*buf), llvm::SMLoc());
        diagEngine = std::make_unique<DiagEngine>(mgr);
        lexer = std::make_unique<Lexer>(mgr, *diagEngine);
    }

    llvm::SourceMgr mgr;
    std::unique_ptr<DiagEngine> diagEngine;
    std::unique_ptr<Lexer> lexer;
};

/*
int aa, b = 4;
aa=1 ;
*/

TEST_F(LexerTest, NextToken) {
    /// 正确集
    /// 当前集
    std::vector<Token> expectedVec, curVec;
    expectedVec.push_back(Token{TokenType::kw_int, 1, 1});
    expectedVec.push_back(Token{TokenType::identifier, 1, 5});
    expectedVec.push_back(Token{TokenType::comma, 1, 7});
    expectedVec.push_back(Token{TokenType::identifier, 1, 9});
    expectedVec.push_back(Token{TokenType::equal, 1, 11});
    expectedVec.push_back(Token{TokenType::number, 1, 13});
    expectedVec.push_back(Token{TokenType::semi, 1, 14});
    expectedVec.push_back(Token{TokenType::identifier, 2, 1});
    expectedVec.push_back(Token{TokenType::equal, 2, 3});
    expectedVec.push_back(Token{TokenType::number, 2, 4});
    expectedVec.push_back(Token{TokenType::semi, 2, 6});

    Token tok;
    while (true) {
        lexer->NextToken(tok);
        if (tok.tokenType == TokenType::eof)
            break;
        curVec.push_back(tok);
    }

    ASSERT_EQ(expectedVec.size(), curVec.size());
    for (int i = 0; i < expectedVec.size(); i++) {
        const auto &expected_tok = expectedVec[i];
        const auto &cur_tok = curVec[i];

        EXPECT_EQ(expected_tok.tokenType, cur_tok.tokenType);
        EXPECT_EQ(expected_tok.row, cur_tok.row);
        EXPECT_EQ(expected_tok.col, cur_tok.col);
    }
}