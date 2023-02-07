#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>
#include <errno.h>

#include "golpe.h"

#include "constants.h"


static void dbCheck(lmdb::txn &txn, const std::string &cmd) {
    auto dbTooOld = [&](uint64_t ver) {
        LE << "Database version too old: " << ver << ". Expected version " << CURR_DB_VERSION;
        LE << "You should 'strfry export' your events, delete (or move) the DB files, and 'strfry import' them";
        throw herr("aborting: DB too old");
    };

    auto dbTooNew = [&](uint64_t ver) {
        LE << "Database version too new: " << ver << ". Expected version " << CURR_DB_VERSION;
        LE << "You should upgrade your version of 'strfry'";
        throw herr("aborting: DB too new");
    };

    auto s = env.lookup_Meta(txn, 1);

    if (!s) {
        {
            // The first version of the DB didn't use a Meta entry -- we consider this version 0

            bool eventFound = false;

            env.foreach_Event(txn, [&](auto &ev){
                eventFound = true;
                return false;
            });

            if (cmd == "export") return;
            if (eventFound) dbTooOld(0);
        }

        env.insert_Meta(txn, CURR_DB_VERSION, 1);
        return;
    }

    if (s->endianness() != 1) throw herr("DB was created on a machine with different endianness");

    if (s->dbVersion() < CURR_DB_VERSION) {
        if (cmd == "export") return;
        dbTooOld(s->dbVersion());
    }

    if (s->dbVersion() > CURR_DB_VERSION) {
        dbTooNew(s->dbVersion());
    }
}

static void setRLimits() {
    if (!cfg().relay__nofiles) return;
    struct rlimit curr;

    if (getrlimit(RLIMIT_NOFILE, &curr)) throw herr("couldn't call getrlimit: ", strerror(errno));

    if (cfg().relay__nofiles > curr.rlim_max) throw herr("Unable to set NOFILES limit to ", cfg().relay__nofiles, ", exceeds max of ", curr.rlim_max);

    curr.rlim_cur = cfg().relay__nofiles;

    if (setrlimit(RLIMIT_NOFILE, &curr)) throw herr("Failed setting NOFILES limit to ", cfg().relay__nofiles, ": ", strerror(errno));
}



quadrable::Quadrable getQdbInstance(lmdb::txn &txn) {
    quadrable::Quadrable qdb;
    qdb.init(txn);
    qdb.checkout("events");
    return qdb;
}

quadrable::Quadrable getQdbInstance() {
    auto txn = env.txn_ro();
    return getQdbInstance(txn);
}

void onAppStartup(lmdb::txn &txn, const std::string &cmd) {
    dbCheck(txn, cmd);

    setRLimits();

    (void)getQdbInstance(txn);
}
