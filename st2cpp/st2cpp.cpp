#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <QtCore/QRegularExpression>
#include <spdlog/spdlog.h>
#include "grammar.h"

std::shared_ptr<spdlog::logger> logr;
#define LogDBG(...) logr->debug(__VA_ARGS__)
#define LogERR(...) logr->error(__VA_ARGS__)

class Grammar
{
	Grammar()
	:kwmap(grammar::keywordsMap())
	,blockmap(grammar::blockMap())
	{
	}

public:
	static Grammar& inst()
	{
		static Grammar _;
		return _;
	}
	const grammar::KwMap kwmap;
	const grammar::BlockMap blockmap;

	bool isBlock(grammar::kw kw) const
	{
		auto res = blockmap.find(kw);
		return res != std::cend(blockmap);
	}

	grammar::kw blockEnd(grammar::kw block) const
	{
		return blockmap.at(block);
	}
};

struct Token
{
	enum class Type
	{
		ws, comment_block, comment_line, kw, oper, ident, liter
	};

	Type type;
	std::string s;
	std::size_t srcline;
	grammar::kw kw() const
	{
		return Grammar::inst().kwmap.at(s);
	}

	void dump() const
	{
		switch(type)
		{
		case Type::ws:
			LogDBG("blank line: '{}'", s);
			break;
		case Type::comment_line:
			LogDBG("comment line: --{}", s);
			break;
		case Type::comment_block:
			LogDBG("comment block: /*{}*/", s);
			break;
		case Type::kw:
			LogDBG("kw: ={}='", s);
			break;
		case Type::oper:
			LogDBG("oper: x {} x", s);
			break;
		case Type::ident:
			LogDBG("ident: = {} =", s);
			break;
		case Type::liter:
			LogDBG("liter: = {} =", s);
			break;
		default:
			LogERR("token dunp fuckup");
			break;
		}
	}
};

class Parser
{
	std::string left;
	std::string line;
	std::size_t lineNr = 0;

public:
	struct Chunk
	{
		enum class Type { EF, BLANK, WORD , NonW };
		Type type = Type::EF;
		std::string s;
	};

	Chunk chunk()
	{
		if(left.empty())
		{
			if( not std::getline(std::cin, line)){
				return Chunk();
			}
			if(line.empty()){
				return Chunk({Chunk::Type::BLANK, "\n"});
			}
			left = line;
			++lineNr;
		}

		QRegularExpression word("^(\\w[\\w\\d#]*)(.*)");
		auto match = word.match(QString::fromStdString(left));
		if(match.hasMatch()){
			left = match.captured(2).toStdString();
			return Chunk({Chunk::Type::WORD, match.captured(1).toStdString()});
		}

		word.setPattern("^(\\s+)(.*)");
		match = word.match(QString::fromStdString(left));
		if(match.hasMatch()){
			left = match.captured(2).toStdString();
			return Chunk({Chunk::Type::BLANK, match.captured(1).toStdString()});
		}

		word.setPattern("^([^\\w\\s]+)(.*)");
		match = word.match(QString::fromStdString(left));
		if(match.hasMatch()){
			left = match.captured(2).toStdString();
			return Chunk({Chunk::Type::NonW, match.captured(1).toStdString()});
		}

		throw std::invalid_argument("document fuckup");

		return Chunk();
	}

	std::string getRestOfLine()
	{
		return std::move(left);
	}

	std::string getfLine() const
	{
		return line;
	}

	std::size_t getfLineNr() const
	{
		return lineNr;
	}
};

std::vector<Token> parse(Parser& p)
{
	Parser::Chunk chunk;
	std::vector<Token> tv;

	while(1){
		chunk = p.chunk();
		if(chunk.type == Parser::Chunk::Type::EF)
			break;

		if(chunk.type == Parser::Chunk::Type::BLANK){
			tv.emplace_back(Token({Token::Type::ws, chunk.s, p.getfLineNr()}));
		}
		else if(chunk.type == Parser::Chunk::Type::WORD){
			auto res = Grammar::inst().kwmap.find(chunk.s);
			if(res != std::cend(Grammar::inst().kwmap))
			{
				tv.emplace_back(Token({Token::Type::kw, std::move(chunk.s), p.getfLineNr()}));
			}
			else{
				QRegularExpression re("^\\w[\\w\\d]*$");
				auto qstr = QString::fromStdString(chunk.s);
				auto identif = re.match(qstr);
				if(identif.hasMatch()){
					tv.emplace_back(Token({Token::Type::ident, qstr.toLower().toStdString(), p.getfLineNr()}));
				}
				else{
					tv.emplace_back(Token({Token::Type::liter, std::move(chunk.s), p.getfLineNr()}));
				}
			}
		}
		else if(chunk.type == Parser::Chunk::Type::NonW){
			auto& s = chunk.s;

			while(not s.empty())
			{
			  if(0 == s.compare(0,2,"//"))
			  {
				tv.emplace_back(Token({Token::Type::comment_line, s.substr(2)+p.getRestOfLine(), p.getfLineNr()}));
				s = std::string();
			  }
			  else if(0 == s.compare(0,2,"(*")){
				std::string vc;
				s.erase(0,2);

				auto isEOComment = [&chunk,&vc]{
					if(chunk.type == Parser::Chunk::Type::EF)
					{
						throw std::invalid_argument("fuckup end of block comment missing");
					}
					if(chunk.type != Parser::Chunk::Type::NonW)
					{
						vc.append(std::move(chunk.s));
						return false;
					}

					auto res = chunk.s.find("*)");
					if( res != std::string::npos)
					{
						vc.append(chunk.s.substr(0,res));
						chunk.s.erase(0,res+2);
						return true;
					}
					return false;
				};

				while(not isEOComment()){
					chunk = p.chunk();
				}

				tv.emplace_back(Token({Token::Type::comment_block, std::move(vc), p.getfLineNr()}));
			  }
			  else if(0 == s.compare(0,1,"'")){
				  std::string strlit("'");
				  s.erase(0,1);

				  auto isEOStrLit = [&chunk,&strlit]{
					  if(chunk.type == Parser::Chunk::Type::EF)
					  {
				  		throw std::invalid_argument("fuckup end of string literal missing");
					  }
					  if(chunk.type != Parser::Chunk::Type::NonW)
					  {
						  strlit.append(std::move(chunk.s));
				  	  	  return false;
				  	  }

				  	  auto res = chunk.s.find("'");
				  	  if( res != std::string::npos)
				  	  {
				  		strlit.append(chunk.s.substr(0,res+1));
				  		chunk.s.erase(0,res+1);
				  		return true;
				  	  }
				  	  return false;
			  	  };

				  while(not isEOStrLit()){
					  chunk = p.chunk();
				  }

			  tv.emplace_back(Token({Token::Type::liter, std::move(strlit), p.getfLineNr()}));

			  }
			  else{
				  auto res = std::find_if(std::cbegin(grammar::nwopers),std::cend(grammar::nwopers),[&chunk](auto& kw){
					  return 0 == chunk.s.compare(0, strlen(kw), kw);
				  });

				  if(res == std::cend(grammar::nwopers))
				  {
					  throw std::invalid_argument("fuckup invalid operator");
				  }
				  else{
					  tv.emplace_back(Token({Token::Type::oper, *res, p.getfLineNr()}));
					  chunk.s.erase(0,strlen(*res));
				  }
			  }
			}
		}
	};

	return tv;
}

struct Tokens
{
	std::size_t cur = 0;
	std::vector<Token> tok;
};

struct Entit;
struct NonCode;
using EntitPtr = std::unique_ptr<Entit>;

struct Entit
{
	enum class Type { UNIT, BLOCK, NONCODE, STATEMENT, VARDEC, COMMAND };
	Entit(Type type) :type(type){}
	std::vector<EntitPtr> subent;
	std::vector<Token> noncode;
	Type type;
	virtual void parseTokens(Tokens& t) = 0;
	virtual ~Entit(){}

	virtual void dump() = 0;
};

struct NonCode : Entit
{
	NonCode():Entit(Type::NONCODE){}
	std::vector<Token> tv;
	void parseTokens(Tokens& t) override {};
	void dump() override
	{
		for(auto& t : tv)
			t.dump();
	}
};

struct Unit : Entit
{
	Unit():Entit(Type::UNIT){}
	Unit(Type type):Entit(type){}
	bool checkNonCode(Token& tok)
	{
		if(tok.type == Token::Type::comment_block ||
		   tok.type == Token::Type::comment_line||
		   tok.type == Token::Type::ws )
		{
			noncode.emplace_back(tok);
			return true;
		}

		if(not noncode.empty())
		{
			auto pp = new NonCode();
			pp->tv = std::move(noncode);
			subent.emplace_back(pp);
		}

		return false;
	}

	void parseTokens(Tokens& t) override;

	void dump() override
	{
		for(auto& e : subent)
			e->dump();
	}
};

struct Command : Unit
{
	std::vector<Token> tv;

	Command() : Unit(Type::COMMAND){}

	void parseTokens(Tokens& t) override
	{
		while(t.cur < t.tok.size())
		{
			auto& tok = t.tok[t.cur++];
			if(tok.type == Token::Type::oper &&
			   tok.s == ";")
			{
					break;
			}
			else
			{
				tv.emplace_back(tok);
			}
		}
	}
};

struct ForBlock : Command
{

};

struct Statement : Unit
{
	Statement() : Unit(Type::STATEMENT){}

	void parseTokens(Tokens& t) override
	{
		while(t.cur < t.tok.size())
		{
			auto& tok = t.tok[t.cur++];
			if(! checkNonCode(tok) ){
				if(tok.type == Token::Type::kw)
				{
					if(tok.kw() == grammar::kw::FOR )
					{
						auto p = new ForBlock();
						p->parseTokens(t);
						subent.emplace_back(p);
					}
					else if(tok.kw() == grammar::kw::IF )
					{
						//ToDo
					}
					else if(tok.kw() == grammar::kw::CASE )
					{
						//ToDo
					}
					else
					{
						LogERR("invalid statement keyword: {}", tok.s );
						throw std::invalid_argument("invalid statement");
					}
				}
				else
				{
					auto p = new Command();
					p->parseTokens(t);
					subent.emplace_back(p);
				}
			}
		}
	}

	void dump() override
	{
	}
};

struct Block : Unit
{
	grammar::kw block;

	Block(grammar::kw kw)
	:Unit(Type::BLOCK)
	,block(kw) {}

	void parseTokens(Tokens& t) override
	{
		while(t.cur < t.tok.size())
		{
			auto& tok = t.tok[t.cur++];
			if(! checkNonCode(tok) ){
				if(tok.type == Token::Type::kw)
				{
					if(Grammar::inst().blockEnd(block) == tok.kw() )
					{
						break;
					}
					else if(Grammar::inst().isBlock(tok.kw()) )
					{
						auto block = new Block(tok.kw());
						block->parseTokens(t);
						subent.emplace_back(block);
						continue;
					}
				}

				auto p = new Statement();
				p->parseTokens(t);
				subent.emplace_back(p);
			}
		}
	}

	void dump() override
	{
		auto res = std::find_if(std::cbegin(Grammar::inst().kwmap), std::cend(Grammar::inst().kwmap),[this](auto& m){
			return m.second == block;
		});

		std::string bname = "UNKNOWN";

		if(res != std::cend(Grammar::inst().kwmap))
			bname = res->first;

		LogDBG("block:{} subent count:{}", bname, subent.size());
	}
};

void Unit::parseTokens(Tokens& t)
{
	while(t.cur < t.tok.size())
	{
		auto& tok = t.tok[t.cur++];

		if(! checkNonCode(tok)) {
			if(tok.type == Token::Type::kw &&
	           Grammar::inst().isBlock(tok.kw()) ) {

				auto block = new Block(tok.kw());
				block->parseTokens(t);
				subent.emplace_back(block);
			}
			else {
				LogERR("unexpected token {} {}", tok.srcline, tok.s);
				throw std::invalid_argument("fuckup in unit");
			}
		}
	}
}

int main(int argc, char **argv) {
	auto logger = spdlog::stderr_logger_st("ST2CPP");
	logger->set_level(spdlog::level::debug);
	logr = logger;
	logger->set_pattern("[%L] %v");

	Parser p;

	try{
		auto tokens = parse(p);
		LogDBG("tokens nr:{}", tokens.size());
		for( auto& t : tokens)
		{
			t.dump();
		}

		Unit u;
		Tokens t({0,std::move(tokens)});
		u.parseTokens(t);
		u.dump();
	}
	catch(std::invalid_argument e){
		LogERR("parse fuckup {} at {}: {}", e.what(), p.getfLineNr(), p.getfLine());
		return 1;
	}

	return 0;
}
