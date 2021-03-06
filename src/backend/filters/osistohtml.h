/*********
*
* In the name of the Father, and of the Son, and of the Holy Spirit.
*
* This file is part of BibleTime's source code, http://www.bibletime.info/.
*
* Copyright 1999-2014 by the BibleTime developers.
* The BibleTime source code is licensed under the GNU General Public License version 2.0.
*
**********/

#ifndef FILTERS_OSISTOHTML_H
#define FILTERS_OSISTOHTML_H

// Sword includes:
#include <osishtmlhref.h>
#include <swbuf.h>
#include <swmodule.h>

namespace Filters {

/**
  \brief OSIS to HTMl conversion filter.
*/
class OsisToHtml: public sword::OSISHTMLHREF {
    protected: /* Types: */
        class UserData: public sword::OSISHTMLHREF::MyUserData {
            public:
                inline UserData(const sword::SWModule *module,
                                const sword::SWKey *key)
                     : sword::OSISHTMLHREF::MyUserData(module, key),
                       swordFootnote(1), inCrossrefNote(false),
                       entryAttributes(module->getEntryAttributes()),
                       noteType(Unknown) {}

                unsigned short int swordFootnote;
                bool inCrossrefNote;
                sword::AttributeTypeList entryAttributes;

                enum NoteType {
                    Unknown,
                    Alternative,
                    CrossReference,
                    Footnote,
                    StrongsMarkup
                } noteType;

                struct {
                    sword::SWBuf who;
                } quote;
        };

    public: /* Methods: */
        OsisToHtml();

        /** Reimplemented from sword::OSISHTMLHREF. */
        virtual bool handleToken(sword::SWBuf &buf,
                                 const char *token,
                                 sword::BasicFilterUserData *userData);

    protected: /* Methods: */
        /** Reimplemented from sword::OSISHTMLHREF. */
        virtual inline sword::BasicFilterUserData *createUserData(
                const sword::SWModule *module,
                const sword::SWKey *key)
        {
            return new UserData(module, key);
        }

    private: /* Methods: */
        void renderReference(const char *osisRef, sword::SWBuf &buf,
                             sword::SWModule *myModule, UserData *myUserData);
};

} // namespace Filters

#endif
