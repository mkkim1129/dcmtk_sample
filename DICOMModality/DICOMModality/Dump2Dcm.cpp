#include "stdafx.h"
#include "Dump2Dcm.h"

#include "dcmtk/config/osconfig.h"

// if defined, use createValueFromTempFile() for large binary data files
//#define EXPERIMENTAL_READ_FROM_FILE

#define INCLUDE_CSTDLIB
#define INCLUDE_CSTDIO
#define INCLUDE_CCTYPE
#include "dcmtk/ofstd/ofstdinc.h"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "dcmtk/ofstd/ofstd.h"
#include "dcmtk/ofstd/ofconapp.h"
#include "dcmtk/ofstd/ofstream.h"
#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/dcmdata/dcpxitem.h"
#include "dcmtk/dcmdata/cmdlnarg.h"
#include "dcmtk/dcmdata/dcuid.h"     /* for dcmtk version name */
#include "dcmtk/dcmdata/dcostrmz.h"  /* for dcmZlibCompressionLevel */

namespace 
{
#define OFFIS_CONSOLE_APPLICATION "dump2dcm"

static OFLogger dump2dcmLogger = OFLog::getLogger("dcmtk.apps." OFFIS_CONSOLE_APPLICATION);

static char rcsid[] = "$dcmtk: " OFFIS_CONSOLE_APPLICATION " v"
	OFFIS_DCMTK_VERSION " " OFFIS_DCMTK_RELEASEDATE " $";

#define SHORTCOL 3
#define LONGCOL 21

// Maximum Line Size (default)

const unsigned int DCM_DumpMaxLineSize = 4096;

// Global variables

static E_ByteOrder opt_fileContentsByteOrdering = EBO_LittleEndian;

// Field definition separators

#define DCM_DumpCommentChar '#'
#define DCM_DumpTagDelim ')'
#define DCM_DumpOpenString '['
#define DCM_DumpCloseString ']'
#define DCM_DumpOpenDescription '('
#define DCM_DumpOpenFile '<'
#define DCM_DumpCloseFile '>'

#define TO_UCHAR(s) OFstatic_cast(unsigned char, (s))
static void
	stripWhitespace(char *s)
{
	char *p;

	if (s == NULL) return;

	p = s;
	while (*s != '\0') {
		if (isspace(TO_UCHAR(*s)) == OFFalse) {
			*p++ = *s;
		}
		s++;
	}
	*p = '\0';
}

static char *
	stripTrailingWhitespace(char *s)
{
	if (s == NULL) return s;
	for (size_t i = strlen(s); i > 0 && isspace(TO_UCHAR(s[--i])); s[i] = '\0');
	return s;
}


static char *
	stripPrecedingWhitespace(char *s)
{
	char *p;
	if (s == NULL) return s;

	for (p = s; *p && isspace(TO_UCHAR(*p)); p++)
		;

	return p;
}

static OFBool
	onlyWhitespace(const char *s)
{
	while (*s) if (!isspace(TO_UCHAR(*s++)))
		return OFFalse;
	return OFTrue;
}

static char *
	getLine(char *line, int maxLineLen, FILE *f, const unsigned long lineNumber)
{
	char *s;

	s = fgets(line, maxLineLen, f);

	// if line is too long, throw rest of it away
	if (s && strlen(s) == size_t(maxLineLen - 1) && s[maxLineLen - 2] != '\n')
	{
		int c = fgetc(f);
		while (c != '\n' && c != EOF)
			c = fgetc(f);
		OFLOG_ERROR(dump2dcmLogger, "line " << lineNumber << " too long");
	}

	/* strip any trailing white space */
	stripTrailingWhitespace(s);

	return s;
}

static OFBool
	isaCommentLine(const char *s)
{
	// skip leading spaces
	while (isspace(TO_UCHAR(*s))) ++s;
	return *s == DCM_DumpCommentChar;
}

static OFBool
	parseTag(char *&s, DcmTagKey &key)
{
	OFBool ok = OFTrue;
	char *p;
	unsigned int g, e;

	// find tag begin
	p = strchr(s, DCM_DumpTagDelim);
	if (p)
	{
		// string all white spaces and read tag
		size_t len = p - s + 1;
		p = new char[len + 1];
		OFStandard::strlcpy(p, s, len + 1);
		stripWhitespace(p);
		s += len;

		if (sscanf(p, "(%x,%x)", &g, &e) == 2)
			key.set(OFstatic_cast(Uint16, g), OFstatic_cast(Uint16, e));
		else
			ok = OFFalse;
		delete[] p;
	}
	else ok = OFFalse;

	return ok;
}


static OFBool
	parseVR(char *&s, DcmEVR &vr)
{
	OFBool ok = OFTrue;

	s = stripPrecedingWhitespace(s);

	// Are there two upper characters?
	if (isupper(TO_UCHAR(*s)) && isupper(TO_UCHAR(*(s + 1))))
	{
		char c_vr[3];
		OFStandard::strlcpy(c_vr, s, 3);
		// Convert to VR
		DcmVR dcmVR(c_vr);
		vr = dcmVR.getEVR();
		s += 2;
	}
	else if ((*s == 'p') && (*(s + 1) == 'i'))
	{
		vr = EVR_pixelItem;
		s += 2;
	}
	else if (((*s == 'o') && (*(s + 1) == 'x')) ||
		((*s == 'x') && (*(s + 1) == 's')) ||
		((*s == 'n') && (*(s + 1) == 'a')) ||
		((*s == 'u') && (*(s + 1) == 'p')))
	{
		// swallow internal VRs
		vr = EVR_UNKNOWN;
		s += 2;
	}
	// dcmdump uses "??" in case of "Unknown Tag & Data" and implicit VR
	else if ((*s == '?') && (*(s + 1) == '?'))
	{
		vr = EVR_UNKNOWN;
		s += 2;
	}
	else ok = OFFalse;

	return ok;
}
#undef TO_UCHAR


static size_t
	searchLastClose(char *s, const char closeChar)
{
	// search last close bracket in a line
	// no bracket structure will be considered

	char *found = NULL;
	char *p = s;

	while (p && *p)
	{
		p = strchr(p, closeChar);
		if (p)
		{
			found = p;
			p++;
		}
	}

	if (found)
		return (found - s) + 1;
	else
		return 0;
}


static size_t
	searchCommentOrEol(char *s)
{
	char *comment = strchr(s, DCM_DumpCommentChar);
	if (comment)
		return comment - s;
	else if (s)
		return strlen(s);
	else
		return 0;
}


static void
	convertNewlineCharacters(char *s)
{
	// convert the string "\n" into the \r\n combination required by DICOM
	if (s) for (; *s; ++s) if (*s == '\\' && *(s + 1) == 'n')
	{
		*s = '\r';
		*++s = '\n';
	}
}

static OFBool
	parseValue(char *&s, char *&value, DcmEVR &vr, const DcmTagKey &tagkey)
{
	OFBool ok = OFTrue;
	size_t len;
	value = NULL;

	s = stripPrecedingWhitespace(s);

	switch (*s)
	{
	case DCM_DumpOpenString:
		len = searchLastClose(s, DCM_DumpCloseString);
		if (len == 0)
			ok = OFFalse;
		else if (len > 2)
		{
			value = new char[len - 1];
			OFStandard::strlcpy(value, s + 1, len - 1);
			convertNewlineCharacters(value);
		}
		else
			value = NULL;
		break;

	case DCM_DumpOpenDescription:
		/* need to distinguish VR=AT from description field */
		/* NB: if the VR is unknown this workaround will not succeed */
		if (vr == EVR_AT)
		{
			len = searchLastClose(s, DCM_DumpTagDelim);
			if (len >= 11)  // (gggg,eeee) allow non-significant spaces
			{
				char *pv = s;
				DcmTagKey tag;
				if (parseTag(pv, tag))   // check for valid tag format
				{
					value = new char[len + 1];
					OFStandard::strlcpy(value, s, len + 1);
					stripWhitespace(value);
				}
				else
					ok = OFFalse;   // skip description
			}
			else
				ok = OFFalse;   // skip description
		}
		/* need to distinguish pixel sequence */
		else if ((tagkey == DCM_PixelData) && (vr == EVR_OB))
		{
			if (strncmp(s + 1, "PixelSequence", 13) == 0)
				vr = EVR_pixelSQ;
		}
		break;

	case DCM_DumpOpenFile:
		ok = OFFalse;  // currently not supported
		break;

	case DCM_DumpCommentChar:
		break;

	default:
		len = searchCommentOrEol(s);
		if (len)
		{
			value = new char[len + 1];
			OFStandard::strlcpy(value, s, len + 1);
			stripTrailingWhitespace(value);
		}
		break;
	}
	return ok;
}

static OFCondition
	putFileContentsIntoElement(DcmElement *elem, const char *filename)
{
	OFCondition ec = EC_Normal;
#ifdef EXPERIMENTAL_READ_FROM_FILE
	/* create stream object for binary file */
	DcmInputFileStream fileStream(filename);
	ec = fileStream.status();
	if (ec.good())
	{
		/* NB: if size is odd file will be rejected */
		const size_t fileLen = OFStandard::getFileSize(filename);
		/* read element value from binary file (requires even length) */
		ec = elem->createValueFromTempFile(fileStream.newFactory(), fileLen, EBO_LittleEndian);
		if (ec.bad())
			OFLOG_ERROR(dump2dcmLogger, "cannot process binary data file: " << filename);
	}
	else {
		OFLOG_ERROR(dump2dcmLogger, "cannot open binary data file: " << filename);
		ec = EC_InvalidTag;
	}
#else
	FILE *f = NULL;
	if ((f = fopen(filename, "rb")) == NULL)
	{
		OFLOG_ERROR(dump2dcmLogger, "cannot open binary data file: " << filename);
		return EC_InvalidTag;
	}

	const size_t len = OFStandard::getFileSize(filename);
	size_t buflen = len;
	if (buflen & 1)
		buflen++; /* if odd then make even (DICOM requires even length values) */

	Uint8 *buf = NULL;
	const DcmEVR evr = elem->getVR();
	/* create buffer of OB or OW data */
	if (evr == EVR_OB || evr == EVR_pixelItem)
		ec = elem->createUint8Array(OFstatic_cast(Uint32, buflen), buf);
	else if (evr == EVR_OW)
	{
		Uint16 *buf16 = NULL;
		ec = elem->createUint16Array(OFstatic_cast(Uint32, buflen / 2), buf16);
		buf = OFreinterpret_cast(Uint8 *, buf16);
	}
	else
		ec = EC_IllegalCall;
	if (ec.good())
	{
		/* read binary file into the buffer */
		if (fread(buf, 1, len, f) != len)
		{
			OFLOG_ERROR(dump2dcmLogger, "error reading binary data file: " << filename
				<< ": " << OFStandard::getLastSystemErrorCode().message());
			ec = EC_CorruptedData;
		}
		else if (evr == EVR_OW)
		{
			/* swap 16 bit OW data (if necessary) */
			swapIfNecessary(gLocalByteOrder, opt_fileContentsByteOrdering, buf, OFstatic_cast(Uint32, buflen), sizeof(Uint16));
		}
	}
	else if (ec == EC_MemoryExhausted)
		OFLOG_ERROR(dump2dcmLogger, "out of memory reading binary data file: " << filename);
	else
		OFLOG_ERROR(dump2dcmLogger, "illegal call processing binary data file: " << filename);

	fclose(f);
#endif
	return ec;
}

static OFCondition
	insertIntoSet(DcmStack &stack, const E_TransferSyntax xfer, const DcmTagKey &tagkey,
		const DcmEVR &vr, const char *value)
{
	// insert new element into dataset or metaheader

	OFCondition l_error = EC_Normal;
	OFCondition newElementError = EC_Normal;

	if (stack.empty())
		l_error = EC_CorruptedData;

	if (l_error == EC_Normal)
	{
		DcmElement *newElement = NULL;
		DcmObject *topOfStack = stack.top();

		// convert tagkey to tag including VR
		DcmTag tag(tagkey);
		DcmVR dcmvr(vr);

		const DcmEVR tagvr = tag.getEVR();
		/* check VR and consider various special cases */
		if (tagvr != vr && vr != EVR_UNKNOWN && tagvr != EVR_UNKNOWN &&
			(tagkey != DCM_LUTData || (vr != EVR_US && vr != EVR_SS && vr != EVR_OW)) &&
			(tagkey != DCM_PixelData || (vr != EVR_OB && vr != EVR_OW && vr != EVR_pixelSQ)) &&
			(tagvr != EVR_xs || (vr != EVR_US && vr != EVR_SS)) &&
			(tagvr != EVR_ox || (vr != EVR_OB && vr != EVR_OW)) &&
			(tagvr != EVR_na || vr != EVR_pixelItem))
		{
			OFLOG_WARN(dump2dcmLogger, "Tag " << tag << " with wrong VR '"
				<< dcmvr.getVRName() << "' found, correct is '"
				<< tag.getVR().getVRName() << "'");
		}
		else if ((tagvr == EVR_UNKNOWN) && (vr == EVR_UNKNOWN))
		{
			OFLOG_WARN(dump2dcmLogger, "VR of Tag " << tag << " is unknown, "
				<< "using 'UN' for this data element");
		}

		if (vr != EVR_UNKNOWN)
			tag.setVR(dcmvr);
		const DcmEVR newTagVR = tag.getEVR();

		// create new element (special handling for pixel sequence and item)
		if (newTagVR == EVR_pixelSQ)
			newElement = new DcmPixelData(tag);
		else if (newTagVR == EVR_pixelItem)
			newElement = new DcmPixelItem(DCM_PixelItemTag);
		else
			newElementError = DcmItem::newDicomElementWithVR(newElement, tag);

		if (newElementError == EC_Normal)
		{
			// tag describes an element
			if (!newElement)
			{
				// Tag was ambiguous - should never happen according to the current implementation of newDicomElement()
				l_error = EC_InvalidVR;
			}
			else
			{
				// check for uncompressed pixel data (i.e. no pixel sequence present)
				if (tagkey == DCM_PixelData && (newTagVR == EVR_OB || newTagVR == EVR_OW))
					OFstatic_cast(DcmPixelData *, newElement)->setNonEncapsulationFlag(OFTrue /*alwaysUnencapsulated*/);
				// fill value
				if (value)
				{
					if (value[0] == '=' && (newTagVR == EVR_OB || newTagVR == EVR_OW || newTagVR == EVR_pixelItem))
					{
						/*
						* Special case handling for OB, OW and pixel item data.
						* Allow a value beginning with a '=' character to represent
						* a file containing data to be used as the attribute value.
						* A '=' character is not a normal value since OB and OW values
						* must be written as multivalued hexadecimal (e.g. "00\ff\0d\8f");
						*/
						l_error = putFileContentsIntoElement(newElement, value + 1);
					}
					else {
						l_error = newElement->putString(value);
					}
				}

				// insert element into hierarchy
				if (l_error == EC_Normal)
				{
					switch (topOfStack->ident())
					{
					case EVR_item:
					case EVR_dirRecord:
					case EVR_dataset:
					case EVR_metainfo:
					{
						DcmItem *item = OFstatic_cast(DcmItem *, topOfStack);
						item->insert(newElement);
						// special handling for pixel sequence
						if (newTagVR == EVR_pixelSQ)
						{
							DcmPixelSequence *pixelSeq = new DcmPixelSequence(DCM_PixelSequenceTag);
							if (pixelSeq != NULL)
							{
								OFstatic_cast(DcmPixelData *, newElement)->putOriginalRepresentation(xfer, NULL, pixelSeq);
								stack.push(pixelSeq);
							}
						}
						else if (newElement->ident() == EVR_SQ)
							stack.push(newElement);
					}
					break;
					case EVR_pixelSQ:
						if (newTagVR == EVR_pixelItem)
						{
							DcmPixelSequence *pixelSeq = OFstatic_cast(DcmPixelSequence *, topOfStack);
							pixelSeq->insert(OFstatic_cast(DcmPixelItem *, newElement));
						}
						else
							l_error = EC_InvalidTag;
						break;
					default:
						l_error = EC_InvalidTag;
						break;
					}
				}
			}
		}
		else if (newElementError == EC_SequEnd)
		{
			// pop stack if stack object was a sequence
			if (topOfStack->ident() == EVR_SQ || topOfStack->ident() == EVR_pixelSQ)
				stack.pop();
			else
				l_error = EC_InvalidTag;
		}
		else if (newElementError == EC_ItemEnd)
		{
			// pop stack if stack object was an item
			switch (topOfStack->ident())
			{
			case EVR_item:
			case EVR_dirRecord:
			case EVR_dataset:
			case EVR_metainfo:
				stack.pop();
				break;

			default:
				l_error = EC_InvalidTag;
				break;
			}
		}
		else if (newElementError == EC_InvalidTag)
		{
			if (tag.getXTag() == DCM_Item)
			{
				DcmItem *item = NULL;
				if (topOfStack->getTag().getXTag() == DCM_DirectoryRecordSequence)
				{
					// an Item must be pushed to the stack
					item = new DcmDirectoryRecord(tag, 0);
					OFstatic_cast(DcmSequenceOfItems *, topOfStack)->insert(item);
					stack.push(item);
				}
				else if (topOfStack->ident() == EVR_SQ)
				{
					// an item must be pushed to the stack
					item = new DcmItem(tag);
					OFstatic_cast(DcmSequenceOfItems *, topOfStack)->insert(item);
					stack.push(item);
				}
				else
					l_error = EC_InvalidTag;
			}
			else
				l_error = EC_InvalidTag;
		}
		else
			l_error = EC_InvalidTag;
	}

	return l_error;
}

static OFBool
	readDumpFile(DcmMetaInfo *metaheader, DcmDataset *dataset,
		FILE *infile, const char *ifname, E_TransferSyntax &xfer,
		const OFBool stopOnErrors, const unsigned long maxLineLength)
{
	char *lineBuf = new char[maxLineLength];
	int lineNumber = 0;
	OFBool errorOnThisLine = OFFalse;
	char *parse = NULL;
	char *value = NULL;
	DcmEVR vr = EVR_UNKNOWN;
	int errorsEncountered = 0;
	DcmTagKey tagkey;
	DcmStack metaheaderStack;
	DcmStack datasetStack;
	xfer = EXS_Unknown;

	if (metaheader)
		metaheaderStack.push(metaheader);

	datasetStack.push(dataset);

	while (getLine(lineBuf, OFstatic_cast(int, maxLineLength), infile, lineNumber + 1))
	{
		lineNumber++;

		OFLOG_TRACE(dump2dcmLogger, "processing line " << lineNumber << " of the input file");

		// ignore empty lines and comment lines
		if (onlyWhitespace(lineBuf))
			continue;
		if (isaCommentLine(lineBuf))
			continue;

		errorOnThisLine = OFFalse;

		parse = &lineBuf[0];

		// parse tag an the line
		if (!parseTag(parse, tagkey))
		{
			OFLOG_ERROR(dump2dcmLogger, OFFIS_CONSOLE_APPLICATION ": " << ifname << ": "
				<< "no Tag found (line " << lineNumber << ")");
			errorOnThisLine = OFTrue;
		}

		// parse optional VR
		if (!errorOnThisLine && !parseVR(parse, vr))
			vr = EVR_UNKNOWN;

		// parse optional value
		if (!errorOnThisLine && !parseValue(parse, value, vr, tagkey))
		{
			OFLOG_ERROR(dump2dcmLogger, OFFIS_CONSOLE_APPLICATION ": " << ifname << ": "
				<< "incorrect value specification (line " << lineNumber << ")");
			errorOnThisLine = OFTrue;
		}


		// insert new element that consists of tag, VR, and value
		if (!errorOnThisLine)
		{
			OFCondition l_error = EC_Normal;
			if (tagkey.getGroup() == 0x0002)
			{
				if (metaheader)
				{
					l_error = insertIntoSet(metaheaderStack, xfer, tagkey, vr, value);
					// check for transfer syntax in meta-header
					if ((tagkey == DCM_TransferSyntaxUID) && (xfer == EXS_Unknown))
					{
						const char *xferUID;
						// use resolved value (UID)
						if (metaheader->findAndGetString(DCM_TransferSyntaxUID, xferUID).good())
							xfer = DcmXfer(xferUID).getXfer();
					}
				}
			}
			else
				l_error = insertIntoSet(datasetStack, xfer, tagkey, vr, value);

			if (value)
			{
				delete[] value;
				value = NULL;
			}
			if (l_error != EC_Normal)
			{
				errorOnThisLine = OFTrue;
				OFLOG_ERROR(dump2dcmLogger, OFFIS_CONSOLE_APPLICATION ": " << ifname << ": Error in creating Element: "
					<< l_error.text() << " (line " << lineNumber << ")");
			}

		}

		if (errorOnThisLine)
			errorsEncountered++;
	}


	// test blocking structure
	if (metaheader && metaheaderStack.card() != 1)
	{
		OFLOG_ERROR(dump2dcmLogger, OFFIS_CONSOLE_APPLICATION ": " << ifname << ": Block Error in metaheader");
		errorsEncountered++;
	}

	if (datasetStack.card() != 1)
	{
		OFLOG_ERROR(dump2dcmLogger, OFFIS_CONSOLE_APPLICATION ": " << ifname << ": Block Error in dataset");
		errorsEncountered++;
	}

	delete[] lineBuf;

	if (errorsEncountered)
	{
		OFLOG_ERROR(dump2dcmLogger, errorsEncountered << " Errors found in " << ifname);
		return !stopOnErrors;
	}
	else
		return OFTrue;
}

static OFBool
readCstring(DcmMetaInfo *metaheader, DcmDataset *dataset, const CString& dump_contents, E_TransferSyntax &xfer,
	const OFBool stopOnErrors)
{
	int lineNumber = 0;
	OFBool errorOnThisLine = OFFalse;
	char *parse = NULL;
	char *value = NULL;
	DcmEVR vr = EVR_UNKNOWN;
	int errorsEncountered = 0;
	DcmTagKey tagkey;
	DcmStack metaheaderStack;
	DcmStack datasetStack;
	xfer = EXS_Unknown;

	if (metaheader)
		metaheaderStack.push(metaheader);

	datasetStack.push(dataset);

	int nTokenPos = 0;
	CString dump_line;
	int i = 0;
	
	while (AfxExtractSubString(dump_line, dump_contents, i))
	{
		lineNumber++;

		OFLOG_TRACE(dump2dcmLogger, "processing line " << lineNumber << " of the input file");

		// ignore empty lines and comment lines
		if (onlyWhitespace(dump_line))
		{
			++i;
			continue;
		}
		if (isaCommentLine(dump_line))
		{
			++i;
			continue;
		}

		errorOnThisLine = OFFalse;

		parse = dump_line.GetBuffer();

		// parse tag an the line
		if (!parseTag(parse, tagkey))
		{
			errorOnThisLine = OFTrue;
		}

		// parse optional VR
		if (!errorOnThisLine && !parseVR(parse, vr))
			vr = EVR_UNKNOWN;

		// parse optional value
		if (!errorOnThisLine && !parseValue(parse, value, vr, tagkey))
		{
			errorOnThisLine = OFTrue;
		}


		// insert new element that consists of tag, VR, and value
		if (!errorOnThisLine)
		{
			OFCondition l_error = EC_Normal;
			if (tagkey.getGroup() == 0x0002)
			{
				if (metaheader)
				{
					l_error = insertIntoSet(metaheaderStack, xfer, tagkey, vr, value);
					// check for transfer syntax in meta-header
					if ((tagkey == DCM_TransferSyntaxUID) && (xfer == EXS_Unknown))
					{
						const char *xferUID;
						// use resolved value (UID)
						if (metaheader->findAndGetString(DCM_TransferSyntaxUID, xferUID).good())
							xfer = DcmXfer(xferUID).getXfer();
					}
				}
			}
			else
				l_error = insertIntoSet(datasetStack, xfer, tagkey, vr, value);

			if (value)
			{
				delete[] value;
				value = NULL;
			}
			if (l_error != EC_Normal)
			{
				errorOnThisLine = OFTrue;
			}

		}

		if (errorOnThisLine)
			errorsEncountered++;

		++i;
	}


	// test blocking structure
	if (metaheader && metaheaderStack.card() != 1)
	{
		errorsEncountered++;
	}

	if (datasetStack.card() != 1)
	{
		errorsEncountered++;
	}

	if (errorsEncountered)
	{
		return !stopOnErrors;
	}
	else
		return OFTrue;
}

}

Dump2Dcm::Dump2Dcm(const CString& dump_contents)
	: m_dump_contents(dump_contents)
{
	m_dump_contents.Replace("\r\n", "\n");
}

Dump2Dcm::~Dump2Dcm()
{
}

void Dump2Dcm::Save(const CString& filename) 
{
	DcmFileFormat fileformat;
	DcmMetaInfo *metaheader = fileformat.getMetaInfo();
	DcmDataset *dataset = fileformat.getDataset();

	E_TransferSyntax xfer;
	readCstring(metaheader, dataset, m_dump_contents, xfer, true);

	E_TransferSyntax opt_xfer = EXS_LittleEndianExplicit;
	E_EncodingType opt_enctype = EET_ExplicitLength;
	E_GrpLenEncoding opt_glenc = EGL_recalcGL;
	E_PaddingEncoding opt_padenc = EPD_withoutPadding;
	E_FileWriteMode opt_writeMode = EWM_fileformat;
	OFCmdUnsignedInt opt_filepad = 0;
	OFCmdUnsignedInt opt_itempad = 0;

	if (fileformat.canWriteXfer(opt_xfer))
	{
		OFCondition l_error = fileformat.saveFile((LPCSTR)filename, opt_xfer, opt_enctype, opt_glenc, opt_padenc,
			OFstatic_cast(Uint32, opt_filepad), OFstatic_cast(Uint32, opt_itempad), opt_writeMode);
	}
}