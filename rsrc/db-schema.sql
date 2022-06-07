-- clean up first

drop table if exists dbentry_property cascade;
drop table if exists property cascade;
drop table if exists dbentry_property_number cascade;
drop table if exists dbentry_property_string cascade;
drop table if exists dbentry_property_boolean cascade;
drop table if exists dbentry_software cascade;
drop table if exists software cascade;
drop table if exists dbentry cascade;

-- software
create table software (
	id serial primary key,
	name varchar not null,
	version varchar not null,
	unique(name, version)
);

-- property
create table property (
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

-- dbentry_property, three variants: text, int and float
create table dbentry_property_number (
	id serial primary key,
	dbentry_id bigint references dbentry on delete cascade deferrable initially deferred,
	property_id bigint references property on delete cascade deferrable initially deferred,
	value double precision not null
);

create table dbentry_property_string (
	id serial primary key,
	dbentry_id bigint references dbentry on delete cascade deferrable initially deferred,
	property_id bigint references property on delete cascade deferrable initially deferred,
	value varchar not null
);

create table dbentry_property_boolean (
	id serial primary key,
	dbentry_id bigint references dbentry on delete cascade deferrable initially deferred,
	property_id bigint references property on delete cascade deferrable initially deferred,
	value boolean not null
);

-- permissions

alter table
	software owner to "${owner}";

alter table
	property owner to "${owner}";

alter table
	dbentry owner to "${owner}";

alter table
	dbentry_software owner to "${owner}";

alter table
	dbentry_property_number owner to "${owner}";

alter table
	dbentry_property_string owner to "${owner}";

alter table
	dbentry_property_boolean owner to "${owner}";
