static int
SCAN_FUNC_NAME (lisp_stream_t *stream)
{
    static char *delims = "\"();";

    SCAN_DECLS

    int c;

    do
    {
	c = NEXT_CHAR;
	if (c == EOF)
	    RETURN(TOKEN_EOF);
	else if (c == ';')     	 /* comment start */
	    while (1)
	    {	
		c = NEXT_CHAR;
		if (c == EOF)		
		    RETURN(TOKEN_EOF);
		else if (c == '\n')   	
		    break;
	    }
    } while (isspace(c));

    switch (c)
    {
	case '(' :
	    RETURN(TOKEN_OPEN_PAREN);

	case ')' :
	    RETURN(TOKEN_CLOSE_PAREN);

	case '"' :
	    _token_clear();
	    while (1)
	    {
		c = NEXT_CHAR;
		if (c == EOF)
		    RETURN(TOKEN_ERROR);
		if (c == '"')
		    break;
		if (c == '\\')
		{
		    c = NEXT_CHAR;

		    switch (c)
		    {
			case EOF :
			    RETURN(TOKEN_ERROR);
			
			case 'n' :
			    c = '\n';
			    break;

			case 't' :
			    c = '\t';
			    break;
		    }
		}

		_token_append(c);
	    }
	    RETURN(TOKEN_STRING);

	case '#' :
	    c = NEXT_CHAR;
	    if (c == EOF)
		RETURN(TOKEN_ERROR);

	    switch (c)
	    {
		case 't' :
		    RETURN(TOKEN_TRUE);

		case 'f' :
		    RETURN(TOKEN_FALSE);

		case '?' :
		    c = NEXT_CHAR;
		    if (c == EOF)
			RETURN(TOKEN_ERROR);

		    if (c == '(')
			RETURN(TOKEN_PATTERN_OPEN_PAREN);
		    else
			RETURN(TOKEN_ERROR);
	    }
	    RETURN(TOKEN_ERROR);

	default :
	    if (isdigit(c) || c == '-')
	    {
		int have_nondigits = 0;
		int have_digits = 0;
		int have_floating_point = 0;

		TOKEN_START(1);

		do
		{
		    if (isdigit(c))
		        have_digits = 1;
		    else if (c == '.')
		        have_floating_point++;
		    TOKEN_APPEND(c);

		    c = NEXT_CHAR;

		    if (c != EOF && !isdigit(c) && !isspace(c) && c != '.' && !strchr(delims, c))
			have_nondigits = 1;
		} while (c != EOF && !isspace(c) && !strchr(delims, c));

		if (c != EOF)
		    UNGET_CHAR(c);

		TOKEN_STOP;

		if (have_nondigits || !have_digits || have_floating_point > 1)
		    RETURN(TOKEN_SYMBOL);
		else if (have_floating_point == 1)
		    RETURN(TOKEN_REAL);
		else
		    RETURN(TOKEN_INTEGER);
	    }
	    else
	    {
		if (c == '.')
		{
		    c = NEXT_CHAR;
		    if (c != EOF && !isspace(c) && !strchr(delims, c))
		    {
			TOKEN_START(2);
			TOKEN_APPEND('.');
		    }
		    else
		    {
			UNGET_CHAR(c);
			RETURN(TOKEN_DOT);
		    }
		}
		else
		{
		    TOKEN_START(1);
		}
		do
		{
		    TOKEN_APPEND(c);
		    c = NEXT_CHAR;
		} while (c != EOF && !isspace(c) && !strchr(delims, c));
		if (c != EOF)
		    UNGET_CHAR(c);

		TOKEN_STOP;

		RETURN(TOKEN_SYMBOL);
	    }
    }

    assert(0);
    RETURN(TOKEN_ERROR);
}
