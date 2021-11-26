#!/bin/bash -x

ldapadd -x -H ldap://192.168.233.16 -D "cn=admin,dc=example,dc=org" -wadmin -f /ldap/usr1.ldif
# wait for ldap server to do smt
sleep 1
ldapadd -x -H ldap://192.168.233.16 -D "cn=admin,dc=example,dc=org" -wadmin -f /ldap/usr3.ldif
# wait for ldap server to do smt
sleep 1

/usr/bin/odyssey /ldap/odyssey.conf

PGPASSWORD=lolol psql -h localhost -p 6432 -U user1 -c "select 1" ldap_db >/dev/null 2>&1 || {
    echo "error: failed to successfully auth with correct password"
    ody-stop
    cat /var/log/odyssey.log
    exit 1
}

PGPASSWORD=notlolol psql -h localhost -p 6432 -U user1 -c "select 1" ldap_db >/dev/null 2>&1 && {
    echo "error:  successfully auth with incorrect password"
    ody-stop
    cat /var/log/odyssey.log
    exit 1
}

PGPASSWORD=default psql -h localhost -p 6432 -U user3 -c "select 1" ldap_db >/dev/null 2>&1 && {
    echo "error: failed to successfully auth with correct password as default user"
    ody-stop
    cat /var/log/odyssey.log
    exit 1
}

PGPASSWORD=notdefault psql -h localhost -p 6432 -U user3 -c "select 1" ldap_db >/dev/null 2>&1 && {
    echo "error:  successfully auth with incorrect password as default user"
    ody-stop
    cat /var/log/odyssey.log
    exit 1
}

ody-stop
