import re, sys, string

debug = True
dict = { }
tokens = [ ]


#  Expression class and its subclasses
class Expression( object ):
	def __str__(self):
		return "" 
	
class BinaryExpr( Expression ):
	def __init__(self, op, left, right):
		self.op = op
		self.left = left
		self.right = right
		
	def __str__(self):
		return str(self.op) + " " + str(self.left) + " " + str(self.right)

class Number( Expression ):
	def __init__(self, value):
		self.value = value
		
	def __str__(self):
		return str(self.value)


class VarRef( Expression ):
	def __init__(self, value):
		self.value = value

	def __str__(self):
		return str(self.value)

# needed for variable names
class String( Expression ):
	def __init__(self, value):
		self.value = value

	def __str__(self):
		return '"' + str(self.value) + '"'


class Statement( object ):
	def __init__(self, stmt):
		self.stmt = stmt

	def __str__(self):
		return str(self.stmt) + "\n"

class StatementList( Statement ):
	def __init__(self, statements):
		self.statements = statements

	def __str__(self):
		stmtStrs = ''
		for stmt in self.statements:
			stmtStrs += str(stmt)
		return stmtStrs

class AssignStmt(Statement):
	def __init__(self, left, right):
		self.left = left
		self.right = right

	def __str__(self):
		return "=" + " " + str(self.left) + " " + str(self.right)

class WhileStmt(Statement):
	def __init__(self, expr, block):
		self.expr = expr
		self.block = block

	def __str__(self):
		return "while " + str(self.expr) + "\n" + str(self.block) + "endwhile"

class IfStmt(Statement):
	def __init__(self, expr, block, elseBlock):
		self.expr = expr
		self.block = block
		self.elseBlock = elseBlock

	def __str__(self):
		ifStr = "if " + str(self.expr) + "\n" + str(self.block)
		if self.elseBlock:
			elseStr = "else\n" + str(self.elseBlock)
			return ifStr + elseStr + "endif"
		return  ifStr + "endif"

class BlockStmt(Statement):
	def __init__(self, stmtList):
		self.stmtList = stmtList

	def __str__(self):
		return str(self.stmtList)

def error( msg, location ):
	#print msg
	sys.exit(msg + ": " + location)

# The "parse" function. This builds a list of tokens from the input string,
# and then hands it to a recursive descent parser for the PAL grammar.

def match(matchtok):
	tok = tokens.peek( )
	if (tok != matchtok): error("Expecting "+ matchtok)
	tokens.next( )
	return tok
	
def factor( ):
	"""factor     = number |  '(' expression ')' """

	tok = tokens.peek( )
	if debug: print ("Factor: ", tok)
	if re.match(Lexer.number, tok):
		expr = Number(tok)
		tokens.next( )
		return expr
	if re.match(Lexer.string, tok):
		expr = String(tok)
		tokens.next( )
		return expr
	if re.match(Lexer.identifier, tok):
		expr = VarRef(tok)
		tokens.next( )
		return expr
	if tok == "(":
		tokens.next( )  # or match( tok )
		expr = addExpr( )
		tokens.peek( )
		tok = match(")")
		return expr
	error("Invalid operand", "end of factor")
	return

def relationalExpr():
	tok = tokens.peek( )
	if debug: print ("relationalExpr: ", tok)
	left = addExpr()
	tok = tokens.peek( )
	if re.match(Lexer.relational, tok): # if for square brackets, while for curly braces
		tokens.next()
		right = relationalExpr()
		left = BinaryExpr(tok, left, right)
		return left
	return left


def andExpr():
	tok = tokens.peek( )
	if debug: print ("andExpr: ", tok)
	left = relationalExpr()
	tok = tokens.peek( )
	while tok == "and":
		tokens.next()
		right = relationalExpr()
		left = BinaryExpr(tok, left, right)
		tok = tokens.peek()
	return left

# choose among productions for LHS based on First+ sets of LHS
# First+(LHS->RHS) is either First(RHS) if EOL is not in RHS, or First(RHS) U Follow(LHS) if it is
# First(NT) is the set of terminal symobls that can appear as the first word in some string derived from LHS
# First(T) is just T
# if the first symbol on RHS is a non termninal, call its function to get the terminals
# First+(expression) = andExpr, there is only 1 production for expression
def parseExpr():
	tok = tokens.peek( )
	if debug: print ("expression: ", tok)
	left = andExpr()
	tok = tokens.peek( )
	while tok == "or":
		tokens.next()
		right = andExpr()
		left = BinaryExpr(tok, left, right)
		tok = tokens.peek( )
	return left

def assign():
	tok = tokens.peek( )
	if debug: print ("assign: ", tok)
	if re.match(Lexer.identifier, tok):
		left = tok
		tokens.next()
		tok = tokens.peek( )
		if tok == "=":
			tokens.next()
		else:
			error("Invalid operand", "assign missing =")
		right = parseExpr()
		tok = tokens.peek( )
		if tok == ";":
			tokens.next()
		else:
			error("Invalid operand", "assign missing ;")
		return AssignStmt(left, right)

def whileStmt():
	tok = tokens.peek( )
	if debug: print ("while: ", tok)
	if tok == "while":
		tok = tokens.next()
		expr = parseExpr()
		blk = block()
		return WhileStmt(expr, blk)

def ifStmt():
	tok = tokens.peek( )
	if debug: print ("if: ", tok)
	if tok == "if":
		tok = tokens.next()
		expr = parseExpr()
		blk = block()
		tok = tokens.peek( )
		elseBlk = None
		if tok == "else":
			tok = tokens.next()
			elseBlk = block()
		return IfStmt(expr, blk, elseBlk)

def block():
	tok = tokens.peek( )
	if debug: print ("block: ", tok)
	if tok == ":":
		tok = tokens.next()
		if tok == ";":
			tok = tokens.next()
			if tok == "@":
				tok = tokens.next()
				stmtList = parseStmtList()
				tok = tokens.peek( )
				if tok == "~":
					tokens.next()
					return BlockStmt(stmtList)
				else:
					error("Invalid operand", "block missing ~")
			else:
				error("Invalid operand", "block missing @")
		else:
			error("Invalid operand", "block missing ;")


def parseStmt():
	tok = tokens.peek( )
	if debug: print ("statement: ", tok)
	stmt = None
	if tok == "if":
		stmt = ifStmt()
	elif tok == "while":
		stmt = whileStmt()
	elif re.match(Lexer.identifier, tok):
		stmt = assign()
	else:
		error("Invalid operand", "invalid statement")
	return Statement(stmt)

def term( ):
	""" term    = factor { ('*' | '/') factor } """

	tok = tokens.peek( )
	if debug: print ("Term: ", tok)
	left = factor( )
	tok = tokens.peek( )
	while tok == "*" or tok == "/":
		tokens.next()
		right = factor( )
		left = BinaryExpr(tok, left, right)
		tok = tokens.peek( )
	return left

def addExpr( ):
	""" addExpr    = term { ('+' | '-') term } """

	tok = tokens.peek( )
	if debug: print ("addExpr: ", tok)
	left = term( )
	tok = tokens.peek( )
	while tok == "+" or tok == "-":
		tokens.next()
		right = term( )
		left = BinaryExpr(tok, left, right)
		tok = tokens.peek( )
	return left

# each line in a program is a statement
def parseStmtList(  ):
	""" gee = { Statement } """
	tok = tokens.peek( )
	stmts = []
	while tok is not None and tok != '~': # end of block
		# need to store each statement in a list		
		ast = parseStmt()
		stmts.append(ast)
		tok = tokens.peek( )
	stmtList = StatementList(stmts)
	return stmtList

def parse( text ) :
	global tokens
	tokens = Lexer( text )
	#expr = addExpr( )
	#print (str(expr))
	#     Or:
	stmtlist = parseStmtList( )
	print(str(stmtlist))
	return


# Lexer, a private class that represents lists of tokens from a Gee
# statement. This class provides the following to its clients:
#
#   o A constructor that takes a string representing a statement
#       as its only parameter, and that initializes a sequence with
#       the tokens from that string.
#
#   o peek, a parameterless message that returns the next token
#       from a token sequence. This returns the token as a string.
#       If there are no more tokens in the sequence, this message
#       returns None.
#
#   o removeToken, a parameterless message that removes the next
#       token from a token sequence.
#
#   o __str__, a parameterless message that returns a string representation
#       of a token sequence, so that token sequences can print nicely

class Lexer :
	
	
	# The constructor with some regular expressions that define Gee's lexical rules.
	# The constructor uses these expressions to split the input expression into
	# a list of substrings that match Gee tokens, and saves that list to be
	# doled out in response to future "peek" messages. The position in the
	# list at which to dole next is also saved for "nextToken" to use.
	
	special = r"\(|\)|\[|\]|,|:|;|@|~|;|\$"
	relational = "<=?|>=?|==?|!="
	arithmetic = "\+|\-|\*|/"
	#char = r"'."
	string = r"'[^']*'" + "|" + r'"[^"]*"' # something enclosed in single or double quotes, this might catch if|while|else
	number = r"\-?\d+(?:\.\d+)?"
	literal = string + "|" + number
	#idStart = r"a-zA-Z"
	#idChar = idStart + r"0-9"
	#identifier = "[" + idStart + "][" + idChar + "]*"
	identifier = "[a-zA-Z]\w*"
	statements = "if|while|else"
	lexRules = literal + "|" + special + "|" + relational + "|" + arithmetic + "|" + identifier + "|" + statements
	
	def __init__( self, text ) :
		self.tokens = re.findall( Lexer.lexRules, text )
		self.position = 0
		self.indent = [ 0 ]
	
	
	# The peek method. This just returns the token at the current position in the
	# list, or None if the current position is past the end of the list.
	
	def peek( self ) :
		if self.position < len(self.tokens) :
			return self.tokens[ self.position ]
		else :
			return None
	
	
	# The removeToken method. All this has to do is increment the token sequence's
	# position counter.
	
	def next( self ) :
		self.position = self.position + 1
		return self.peek( )
	
	
	# An "__str__" method, so that token sequences print in a useful form.
	
	def __str__( self ) :
		return "<Lexer at " + str(self.position) + " in " + str(self.tokens) + ">"



def chkIndent(line):
	ct = 0
	for ch in line:
		if ch != " ": return ct
		ct += 1
	return ct
		

def delComment(line):
	pos = line.find("#")
	if pos > -1:
		line = line[0:pos]
		line = line.rstrip()
	return line

def mklines(filename):
	inn = open(filename, "r")
	lines = [ ]
	pos = [0]
	ct = 0
	for line in inn:
		ct += 1
		line = line.rstrip( )+";"
		line = delComment(line)
		if len(line) == 0 or line == ";": continue
		indent = chkIndent(line)
		line = line.lstrip( )
		if indent > pos[-1]:
			pos.append(indent)
			line = '@' + line
		elif indent < pos[-1]:
			while indent < pos[-1]:
				del(pos[-1])
				line = '~' + line
		print (ct, "\t", line)
		lines.append(line)
	# print len(pos)
	undent = ""
	for i in pos[1:]:
		undent += "~"
	lines.append(undent)
	# print undent
	return lines



def main():
	"""main program for testing"""
	global debug
	ct = 0
	for opt in sys.argv[1:]:
		if opt[0] != "-": break
		ct = ct + 1
		if opt == "-d": debug = True
	if len(sys.argv) < 2+ct:
		print ("Usage:  %s filename" % sys.argv[0])
		return
	parse("".join(mklines(sys.argv[1+ct])))
	return


main()
