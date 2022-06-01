-- clean up first

drop table if exists dbentry_properties_int;
drop table if exists dbentry_properties_text;
drop table if exists dbentry_properties_float;
drop table if exists properties;
drop table if exists dbentry_software;
drop table if exists software;
drop table if exists dbentry;

-- software
create table software (
	id serial primary key,
	name varchar not null,
	version varchar not null,
	unique(name, version)
);

-- properties
create table properties (
	id serial primary key,
	name varchar not null,
	unique(name)
);

-- dbentry
create table dbentry (
	id serial primary key,
	pdb_id varchar not null,
	version_hash varchar not null,

	coordinates_revision_date_pdb date not null,
	coordinates_revision_major_mmCIF varchar,
	coordinates_revision_minor_mmCIF varchar,
	coordinates_edited boolean,
	reflections_revision varchar,
	reflections_edited boolean,

	created timestamp with time zone default current_timestamp not null,
	unique(pdb_id, version_hash)
);

-- dbentry_software
create table dbentry_software (
	id serial primary key,
	dbentry_id bigint references dbentry on delete cascade deferrable initially deferred,
	software_id bigint references software on delete cascade deferrable initially deferred,
	used boolean
);

-- dbentry_properties, three variants: text, int and float
create table dbentry_properties_int (
	id serial primary key,
	dbentry_id bigint references dbentry on delete cascade deferrable initially deferred,
	property_id bigint references properties on delete cascade deferrable initially deferred,
	value bigint not null
);

create table dbentry_properties_text (
	id serial primary key,
	dbentry_id bigint references dbentry on delete cascade deferrable initially deferred,
	property_id bigint references properties on delete cascade deferrable initially deferred,
	value varchar not null
);

create table dbentry_properties_float (
	id serial primary key,
	dbentry_id bigint references dbentry on delete cascade deferrable initially deferred,
	property_id bigint references properties on delete cascade deferrable initially deferred,
	value double precision not null
);

-- permissions

alter table
	software owner to "${owner}";

alter table
	properties owner to "${owner}";

alter table
	dbentry owner to "${owner}";

alter table
	dbentry_software owner to "${owner}";

alter table
	dbentry_properties_int owner to "${owner}";

alter table
	dbentry_properties_text owner to "${owner}";

alter table
	dbentry_properties_float owner to "${owner}";