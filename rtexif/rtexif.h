/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 * 
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _MEXIF3_
#define _MEXIF3_

#include <cstdio>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cmath>
#include "../rtengine/procparams.h"
#include "../rtengine/safekeyfile.h"

class CacheImageData;

namespace rtexif {

enum TagType {INVALID=0, BYTE=1, ASCII=2, SHORT=3, LONG=4, RATIONAL=5, UNDEFINED=7, SSHORT=8, SLONG=9, SRATIONAL=10, FLOAT=11, DOUBLE=12, OLYUNDEF=13, AUTO=98, SUBDIR=99};
enum ActionCode {
    AC_DONTWRITE,  // don't write it to the output
    AC_WRITE,      // write it to the output
    AC_SYSTEM,     // changed by RT (not editable/deletable) - don't write, don't show
    AC_NEW,        // new addition - write, don't show

    AC_INVALID=100,    // invalid state
};
enum ByteOrder {INTEL=0x4949, MOTOROLA=0x4D4D};
enum MNKind {NOMK, IFD, HEADERIFD, NIKON3, OLYMPUS2, FUJI,TABLESUBDIR};

bool extractLensInfo(std::string &fullname,double &minFocal, double &maxFocal, double &maxApertureAtMinFocal, double &maxApertureAtMaxFocal);

unsigned short sget2 (unsigned char *s, ByteOrder order);
int sget4 (unsigned char *s, ByteOrder order);
inline unsigned short get2 (FILE* f, ByteOrder order);
inline int get4 (FILE* f, ByteOrder order);
inline void sset2 (unsigned short v, unsigned char *s, ByteOrder order);
inline void sset4 (int v, unsigned char *s, ByteOrder order);
inline float int_to_float (int i);
short int int2_to_signed (short unsigned int i);

struct TIFFHeader {

  unsigned short byteOrder;
  unsigned short fixed;
  unsigned int   ifdOffset;
};

class Tag;
class Interpreter;

/// Structure of informations describing an Exif tag
struct TagAttrib {
    int                 ignore;   // =0: never ignore, =1: always ignore, =2: ignore if the subdir type is reduced image, =-1: end of table
    ActionCode          action;
    int                 editable;
    const  TagAttrib*   subdirAttribs;  // !NULL if this tag points to a subdir
    /**  Numeric identifier of tag (or index inside DirectoryTable)
         To avoid rewriting all the tables, and to address the problem of TagDirectoryTable with heterogeneous tag's type,
         this parameter is now an unsigned int, where the leftmost 2 bytes represent the tag's type, which by default will be aqual
         to 0 (INVALID). Only non null tag type will be used. See nikon attrib for an example
    */
    unsigned short      ID;
    TagType             type;
    const char*         name;
    Interpreter*        interpreter; // Call back hook
};

const TagAttrib* lookupAttrib (const TagAttrib* dir, const char* field);

/// A directory of tags
class TagDirectory {

  protected:
    std::vector<Tag*> tags;     // tags in the directory
    const TagAttrib*  attribs;  // descriptor table to decode the tags
    ByteOrder         order;    // byte order
    TagDirectory*     parent;   // parent directory (NULL if root)
    static Glib::ustring getDumpKey (int tagID, const Glib::ustring tagName);

  public:
    TagDirectory ();
    TagDirectory (TagDirectory* p, FILE* f, int base, const TagAttrib* ta, ByteOrder border, bool skipIgnored=true);
    TagDirectory (TagDirectory* p, const TagAttrib* ta, ByteOrder border);
    virtual ~TagDirectory ();
   
    inline ByteOrder getOrder      () const { return order; }   
    TagDirectory*    getParent     () { return parent; }
    TagDirectory*    getRoot       ();
    inline int       getCount      () const { return tags.size (); }
    const TagAttrib* getAttrib     (int id);
    const TagAttrib* getAttrib     (const char* name);  // Find a Tag by scanning the whole tag tree and stopping at the first occurrence
    const TagAttrib* getAttribP    (const char* name);  // Try to get the Tag at a given location. 'name' is a path relative to this directory (e.g. "LensInfo/FocalLength")
    const TagAttrib* getAttribTable() { return attribs; }
    Tag*             getTag        (const char* name) const;  // Find a Tag by scanning the whole tag tree and stopping at the first occurrence
    Tag*             getTagP       (const char* name) const;  // Try to get the Tag at a given location. 'name' is a path relative to this directory (e.g. "LensInfo/FocalLength")
    Tag*             getTag        (int ID) const;
    virtual Tag*     findTag       (const char* name) const;
    bool             getXMPTagValue(const char* name, char* value) const;

    void             keepTag       (int ID);
    virtual void     addTag        (Tag* a);
    virtual void     addTagFront   (Tag* a);
    virtual void     replaceTag    (Tag* a);
    inline Tag*      getTagByIndex (int ix) { return tags[ix]; }
    inline void      setOrder      (ByteOrder bo) { order = bo; }   

    virtual int      calculateSize ();
    virtual int      write         (int start, unsigned char* buffer);
    virtual TagDirectory* clone    (TagDirectory* parent);
    virtual void     applyChange   (std::string field, std::string value);

    virtual void     printAll      (unsigned  int level=0) const; // reentrant debug function, keep level=0 on first call !
    virtual bool     CPBDump       (const Glib::ustring &commFName, const Glib::ustring &imageFName, const Glib::ustring &profileFName, const Glib::ustring &defaultPParams,
                                    const CacheImageData* cfs, const bool flagMode, rtengine::SafeKeyFile *keyFile=NULL, Glib::ustring tagDirName="") const;
    virtual void     sort     ();
};

// a table of tags: id are offset from beginning and not identifiers
class TagDirectoryTable: public TagDirectory {
   protected:
	unsigned char *values; // Tags values are saved internally here
	long           zeroOffset; // Offset 0 (index 0) could be at an offset from values
	long           valuesSize; // Size of allocated memory
	TagType        defaultType; // Default type of all tags in this directory
   public:
	TagDirectoryTable();
	TagDirectoryTable (TagDirectory* p, unsigned char *v,int memsize,int offs, TagType type, const TagAttrib* ta, ByteOrder border);
	TagDirectoryTable (TagDirectory* p, FILE* f, int memsize,int offset, TagType type, const TagAttrib* ta, ByteOrder border);
	virtual ~TagDirectoryTable();
	virtual int calculateSize ();
	virtual int write (int start, unsigned char* buffer);
	virtual TagDirectory* clone (TagDirectory* parent);
};

// a class representing a single tag
class Tag {

  protected:
    unsigned short tag;
    TagType        type;
    unsigned int   count;
    unsigned char* value;
    int            valuesize;
    bool           keep;
    bool           allocOwnMemory;

    const TagAttrib* attrib;
    TagDirectory*    parent;
    TagDirectory**   directory;
    MNKind           makerNoteKind;
    bool             parseMakerNote(FILE* f, int base, ByteOrder bom );

  public:
    Tag (TagDirectory* parent, FILE* f, int base);                          // parse next tag from the file
    Tag (TagDirectory* parent, const TagAttrib* attr);
    Tag (TagDirectory* parent, const TagAttrib* attr, unsigned char *data, TagType t);
    Tag (TagDirectory* parent, const TagAttrib* attr, int data, TagType t);  // create a new tag from array (used
    Tag (TagDirectory* parent, const TagAttrib* attr, const char* data);  // create a new tag from array (used
   ~Tag ();
   void initType       (unsigned char *data, TagType type);
   void initInt        (int data, TagType t, int count=1);
   void initString     (const char* text);
   void initSubDir     ();
   void initSubDir     (TagDirectory* dir);
   void initMakerNote  (MNKind mnk, const TagAttrib* ta);
   void initUndefArray (const char* data, int len);
   void initLongArray  (const char* data, int len);
   void initRational   (int num, int den);

    // get basic tag properties
    int                  getID     () const { return tag; }
    int                  getCount  () const { return count; }
    TagType              getType   () const { return (attrib && attrib->type > INVALID && attrib->type < AUTO) ? attrib->type : type; }
    unsigned char*       getValue  () const { return value; }
    const TagAttrib*     getAttrib () const { return attrib; }
    inline ByteOrder     getOrder  () const { return parent ? parent->getOrder() : INTEL; }
    inline TagDirectory* getParent () const { return parent; }
    int                  getValueSize () const { return valuesize; }
    bool                 getOwnMemory() const { return allocOwnMemory; }

    // read/write value
    int     toInt         (int ofs=0, TagType astype=INVALID);
    void    fromInt       (int v);
    double  toDouble      (int ofs=0);
    double *toDoubleArray (int ofs=0);
    void    toRational    (int& num, int& denom, int ofs=0);
    void    toString      (char* buffer, int ofs=0);
    void    fromString    (const char* v, int size=-1);
    void    setInt        (int v, int ofs=0, TagType astype=LONG);


    // additional getter/setter for more comfortable use
    std::string valueToString   ();
    std::string nameToString    (int i=0);
    void        valueFromString (const std::string& value);
    
    // functions for writing
    int  calculateSize ();
    int  write         (int offs, int dataOffs, unsigned char* buffer);
    Tag* clone         (TagDirectory* parent);

    // to control if the tag shall be written 
    bool getKeep ()       { return keep; }
    void setKeep (bool k) { keep = k; }   
   
    // get subdirectory (there can be several, the last is NULL)
    bool           isDirectory  ()        { return directory!=NULL; }
    TagDirectory*  getDirectory (int i=0) { return (directory) ? directory[i] : 0; }

    MNKind getMakerNoteFormat () { return makerNoteKind; }
 };

class ExifManager {

    static std::vector<Tag*> defTags;
  
    static Tag* saveCIFFMNTag (FILE* f, TagDirectory* root, int len, const char* name);
  public:
    static TagDirectory* parse (FILE*f, int base, bool skipIgnored=true);
    static TagDirectory* parseJPEG (FILE*f);
    static TagDirectory* parseTIFF (FILE*f, bool skipIgnored=true);
    static TagDirectory* parseCIFF (FILE* f, int base, int length);
    static void          parseCIFF (FILE* f, int base, int length, TagDirectory* root);
    
    static const std::vector<Tag*>& getDefaultTIFFTags (TagDirectory* forthis);
    static int    createJPEGMarker (const TagDirectory* root, const rtengine::procparams::ExifPairs& changeList, int W, int H, unsigned char* buffer);
    static int    createTIFFHeader (const TagDirectory* root, const rtengine::procparams::ExifPairs& changeList, int W, int H, int bps, const char* profiledata, int profilelen, const char* iptcdata, int iptclen, unsigned char* buffer);
};

class Interpreter {
    public:
        Interpreter () {}
        virtual ~Interpreter() {};
        virtual std::string toString (Tag* t) {
            char buffer[1024];
            t->toString (buffer);
            std::string s(buffer);
            std::string::size_type p1 = s.find_first_not_of(' ');
            if( p1 == std::string::npos )
                return s;
            else
                return s.substr(p1, s.find_last_not_of(' ')-p1+1);
        }
        virtual void fromString (Tag* t, const std::string& value) {
            if (t->getType()==SHORT || t->getType()==LONG)
                t->fromInt (atoi(value.c_str()));
            else
                t->fromString (value.c_str());
        }
        // Get the value as a double
        virtual double toDouble(Tag* t, int ofs=0) {
            double ud, dd;
            switch (t->getType()) {
                case BYTE:  return (double)((int)t->getValue()[ofs]);
                case ASCII: return 0.0;
                case SSHORT:return (double)int2_to_signed(sget2 (t->getValue()+ofs, t->getOrder()));
                case SHORT: return (double)((int)sget2 (t->getValue()+ofs, t->getOrder()));
                case SLONG:
                case LONG:  return (double)((int)sget4 (t->getValue()+ofs, t->getOrder()));
                case SRATIONAL:
                case RATIONAL: ud = (int)sget4 (t->getValue()+ofs, t->getOrder()); dd = (int)sget4 (t->getValue()+ofs+4, t->getOrder()); return dd==0. ? 0. : (double)ud / (double)dd;
                case FLOAT: return double(sget4 (t->getValue()+ofs, t->getOrder()));
                case UNDEFINED: return 0.;
                default: return 0.; // Quick fix for missing cases (INVALID, DOUBLE, OLYUNDEF, SUBDIR)
            }
        }
        // Get the value as an int
        virtual int toInt (Tag* t, int ofs=0, TagType astype=INVALID) {
            int a;
            if (astype == INVALID || astype==AUTO)
                astype = t->getType();
            switch (astype) {
                case BYTE:  return t->getValue()[ofs];
                case ASCII: return 0;
                case SSHORT:return (int)int2_to_signed(sget2 (t->getValue()+ofs, t->getOrder()));
                case SHORT: return (int)sget2 (t->getValue()+ofs, t->getOrder());
                case SLONG:
                case LONG:  return (int)sget4 (t->getValue()+ofs, t->getOrder());
                case SRATIONAL:
                case RATIONAL: a = (int)sget4 (t->getValue()+ofs+4, t->getOrder()); return a==0 ? 0 : (int)sget4 (t->getValue()+ofs, t->getOrder()) / a;
                case FLOAT:     return (int)toDouble(t, ofs);
                case UNDEFINED: return 0;
                default: return 0; // Quick fix for missing cases (INVALID, DOUBLE, OLYUNDEF, SUBDIR)
            }
          return 0;
        }
};

extern Interpreter stdInterpreter;
class ChoiceInterpreter : public Interpreter {
    protected:
        std::map<int,std::string> choices;
    public:
        ChoiceInterpreter () {};
        virtual std::string toString (Tag* t) {
            std::map<int,std::string>::iterator r = choices.find (t->toInt());
            if (r!=choices.end())
                return r->second;
            else {
                char buffer[1024];
                t->toString (buffer);
                return std::string (buffer);
            }
        }
};

template< class T >
class IntLensInterpreter : public Interpreter {
protected:
    typedef std::multimap< T, std::string> container_t;
    typedef typename std::multimap< T, std::string>::iterator it_t;
    typedef std::pair< T, std::string> p_t;
    container_t choices;

    virtual std::string guess(const T lensID, double focalLength, double maxApertureAtFocal, double *lensInfoArray) {
        it_t r;
        size_t nFound = choices.count( lensID );

        switch( nFound ) {
        case 0: // lens Unknown
        {
            std::ostringstream s;
            s << lensID;
            return s.str();
        }
        case 1: // lens found
            r = choices.find ( lensID );
            return r->second;
        default:
            // More than one hit: we must guess
        break;
        }

        std::string bestMatch("Unknown");
        double a1,a2,f1,f2;

        /* FIRST TRY
        *
        * Get the lens info (min/man focal, min/max aperture) and compare them to the possible choice
        */
        if (lensInfoArray) {
            for ( r = choices.lower_bound( lensID ); r != choices.upper_bound(lensID); r++  ){
                if( !extractLensInfo( r->second ,f1,f2,a1,a2) )
                    continue;
                if (f1==lensInfoArray[0] && f2==lensInfoArray[1] && a1==lensInfoArray[2] && a2==lensInfoArray[3])
                    // can't match better! we take this entry as being the one
                    return r->second;
            }
            // No lens found, we update the "unknown" string with the lens info values
            if (lensInfoArray[0]==lensInfoArray[1])
                bestMatch += Glib::ustring::compose(" (%1mm", int(lensInfoArray[0]));
            else
                bestMatch += Glib::ustring::compose(" (%1-%2mm", int(lensInfoArray[0]), int(lensInfoArray[1]));

            if (lensInfoArray[2]==lensInfoArray[3])
                bestMatch += Glib::ustring::compose(" f/%1)", Glib::ustring::format(std::fixed, std::setprecision(1), lensInfoArray[2]));
            else
                bestMatch += Glib::ustring::compose(" f/%1-%2)",
                                       Glib::ustring::format(std::fixed, std::setprecision(1), lensInfoArray[2]),
                                       Glib::ustring::format(std::fixed, std::setprecision(1), lensInfoArray[3]));
        }

        /* SECOND TRY
        *
        * Choose the best match: thanks to exiftool by Phil Harvey
        * first throws for "out of focal range" and lower or upper aperture of the lens compared to MaxApertureAtFocal
        * if the lens is not constant aperture, calculate aprox. aperture of the lens at focalLength
        * and compare with actual aperture.
        */
        std::ostringstream candidates;
        double deltaMin = 1000.;
        for ( r = choices.lower_bound( lensID ); r != choices.upper_bound(lensID); r++  ){
            double lensAperture,dif;

            if( !extractLensInfo( r->second ,f1,f2,a1,a2) )
                continue;
            if( f1 == 0. || a1 == 0.)
                continue;

            if( focalLength < f1 - .5 || focalLength > f2 + 0.5 )
                continue;
            if( maxApertureAtFocal > 0.1){
                if( maxApertureAtFocal < a1 - 0.15 || maxApertureAtFocal > a2 +0.15)
                    continue;

                if( a1 == a2 || f1 == f2)
                lensAperture = a1;
                else
                lensAperture = exp( log(a1)+(log(a2)-log(a1))/(log(f2)-log(f1))*(log(focalLength)-log(f1)) );

                dif = abs(lensAperture - maxApertureAtFocal);
            }else
                dif = 0;
            if( dif < deltaMin ){
                deltaMin = dif;
                bestMatch = r->second;
            }
            if( dif < 0.15){
                if( candidates.tellp() )
                    candidates << "\n or " <<  r->second;
                else
                    candidates <<  r->second;
            }
        }
        if( !candidates.tellp() )
            return bestMatch;
        else
            return candidates.str();
    }
};

inline int getTypeSize( TagType type );

extern const TagAttrib exifAttribs[];
extern const TagAttrib gpsAttribs[];
extern const TagAttrib iopAttribs[];
extern const TagAttrib ifdAttribs[];
extern const TagAttrib nikon2Attribs[];
extern const TagAttrib nikon3Attribs[];
extern const TagAttrib canonAttribs[];
extern const TagAttrib pentaxAttribs[];
extern const TagAttrib pentaxLensDataAttribs[];
extern const TagAttrib pentaxLensInfoQAttribs[];
extern const TagAttrib pentaxAEInfoAttribs[];
extern const TagAttrib pentaxCameraSettingsAttribs[];
extern const TagAttrib pentaxFlashInfoAttribs[];
extern const TagAttrib pentaxSRInfoAttribs[];
extern const TagAttrib pentaxBatteryInfoAttribs[];
extern const TagAttrib pentaxCameraInfoAttribs[];
extern const TagAttrib fujiAttribs[];
extern const TagAttrib minoltaAttribs[];
extern const TagAttrib sonyAttribs[];
extern const TagAttrib sonyTag9405Attribs[];
extern const TagAttrib sonyCameraInfoAttribs[];
extern const TagAttrib sonyCameraInfo2Attribs[];
extern const TagAttrib sonyCameraSettingsAttribs[];
extern const TagAttrib sonyCameraSettingsAttribs2[];
extern const TagAttrib sonyCameraSettingsAttribs3[];
//extern const TagAttrib sonyDNGMakerNote[];
extern const TagAttrib olympusAttribs[];
extern const TagAttrib kodakIfdAttribs[];
void parseKodakIfdTextualInfo(Tag *textualInfo, Tag* exif);
}
#endif
