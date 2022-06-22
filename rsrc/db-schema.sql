-- clean up first
drop view if exists latest_dbentry;

drop view if exists dbentry_software_view;

drop table if exists dbentry_property_number cascade;

drop table if exists dbentry_property_string cascade;

drop table if exists dbentry_property_boolean cascade;

drop table if exists property cascade;

drop table if exists dbentry_software cascade;

drop table if exists software cascade;

drop table if exists dbentry cascade;

-- software
create table software (
	id serial primary key,
	name varchar not null,
	version varchar,
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
	coordinates_revision_date_pdb date,
	coordinates_revision_major_mmCIF varchar,
	coordinates_revision_minor_mmCIF varchar,
	coordinates_edited boolean,
	reflections_revision varchar,
	reflections_edited boolean,
	created timestamp with time zone default current_timestamp not null,
	data_time date not null,
	unique(pdb_id, version_hash)
);

-- a view on dbentry containing only the latest version, based on data_time
create view latest_dbentry as
select
	e.*
from
	dbentry e
where
	e.version_hash = (
		select
			e1.version_hash
		from
			dbentry e1
		where
			e1.pdb_id = e.pdb_id
		order by
			e1.data_time desc
		limit
			1
	);

-- dbentry_software
create table dbentry_software (
	dbentry_id bigint references dbentry on delete cascade deferrable initially deferred,
	software_id bigint references software on delete cascade deferrable initially deferred,
	primary key(dbentry_id, software_id)
);

-- a view on dbentry_software, collapsing the data
create view dbentry_software_view as
select
	es.dbentry_id,
	s.name,
	s.version
from
	dbentry_software es
	join software s on s.id = es.software_id;

-- dbentry_property, three variants: text, int and float
create table dbentry_property_number (
	dbentry_id bigint references dbentry on delete cascade deferrable initially deferred,
	property_id bigint references property on delete cascade deferrable initially deferred,
	value double precision not null,
	primary key(dbentry_id, property_id)
);

create table dbentry_property_string (
	dbentry_id bigint references dbentry on delete cascade deferrable initially deferred,
	property_id bigint references property on delete cascade deferrable initially deferred,
	value varchar not null,
	primary key(dbentry_id, property_id)
);

create table dbentry_property_boolean (
	dbentry_id bigint references dbentry on delete cascade deferrable initially deferred,
	property_id bigint references property on delete cascade deferrable initially deferred,
	value boolean not null,
	primary key(dbentry_id, property_id)
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

alter view
	dbentry_software_view owner to "${owner}";

alter view
	latest_dbentry owner to "${owner}";