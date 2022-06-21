create table entries (
  keyword varchar(200) PRIMARY KEY COLLATE bedic,
  description varchar(1024000),
  create_date int,
  modif_date int );

create table properties (
  tag varchar(200) PRIMARY KEY,
  value varchar(1024000) );

