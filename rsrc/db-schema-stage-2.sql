-- dbentry
alter table
	dbentry
add
	unique(pdb_id, version_hash);

-- dbentry_software
alter table
	dbentry_software
add
	primary key(dbentry_id, software_id);

-- dbentry_property, three variants: text, int and float
alter table
	dbentry_property_number
add
	primary key(dbentry_id, property_id);

alter table
	dbentry_property_string
add
	primary key(dbentry_id, property_id);

alter table
	dbentry_property_boolean
add
	primary key(dbentry_id, property_id);