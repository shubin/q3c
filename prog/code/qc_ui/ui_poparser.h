typedef void ( *po_addmsg )( void *storage, const char *msgid, const char *msgstr );
typedef void ( *po_error ) ( const char *error );
typedef void ( *po_read )( void *file, void *buffer, int amount );

bool po_parse( po_read read, int len, void *file, po_addmsg addmsg, void *storage, po_error error );
