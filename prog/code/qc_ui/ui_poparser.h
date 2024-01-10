// PO file parser (gettext data for localisation)
// returns true if parsed successfully and any other value in any other occasions
bool po_parse( 
	// data read function
	void ( *read )( void *file, void *buffer, int amount ),
	// length of data
	int len,
	// data handle, forwarded to the read function
	void *file,
	// this function is called when a msgid/msgstr pair is successfully parsed
	void ( *addmsg )( void *storage, const char *msgid, const char *msgstr ),
	// this is storage handle, forwarded to the addmsg function
	void *storage,
	// this function is used to provide the caller with the specific error message if any
	void ( *error ) ( const char *error )
);
