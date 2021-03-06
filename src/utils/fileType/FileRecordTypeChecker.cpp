
#include "FileRecordTypeChecker.h"
#include "api/BamReader.h"
#include "ParseTools.h"

FileRecordTypeChecker::FileRecordTypeChecker()
{
	_fileType = UNKNOWN_FILE_TYPE;
	_recordType = UNKNOWN_RECORD_TYPE;
	_numFields = 0;
	_isBinary = false;
	_isText = false;
	_isBed = false;
	_isDelimited = false;
	_delimChar = '\0';
	_lines.clear();
	_firstValidDataLineIdx = -1;
	_isVCF = false;
	_isBAM = false;
	_isGFF = false;
	_isGzipped = false;
	_insufficientData = false;
	_fourthFieldNumeric = false;

	_hasName[UNKNOWN_RECORD_TYPE] = false;
	_hasName[BED3_RECORD_TYPE] = false;
	_hasName[BED6_RECORD_TYPE] = true;
	_hasName[BED12_RECORD_TYPE] = true;
	_hasName[BED_PLUS_RECORD_TYPE] = true;
	_hasName[BAM_RECORD_TYPE] = true;
	_hasName[VCF_RECORD_TYPE] = true;
	_hasName[GFF_RECORD_TYPE] = true;

	_hasScore[UNKNOWN_RECORD_TYPE] = false;
	_hasScore[BED3_RECORD_TYPE] = false;
	_hasScore[BED6_RECORD_TYPE] = true;
	_hasScore[BED12_RECORD_TYPE] = true;
	_hasScore[BED_PLUS_RECORD_TYPE] = true;
	_hasScore[BAM_RECORD_TYPE] = true;
	_hasScore[VCF_RECORD_TYPE] = true;
	_hasScore[GFF_RECORD_TYPE] = true;

	_hasStrand[UNKNOWN_RECORD_TYPE] = false;
	_hasStrand[BED3_RECORD_TYPE] = false;
	_hasStrand[BED6_RECORD_TYPE] = true;
	_hasStrand[BED12_RECORD_TYPE] = true;
	_hasStrand[BED_PLUS_RECORD_TYPE] = true;
	_hasStrand[BAM_RECORD_TYPE] = true;
	_hasStrand[VCF_RECORD_TYPE] = true;
	_hasStrand[GFF_RECORD_TYPE] = true;

	_recordTypeNames[UNKNOWN_RECORD_TYPE] = "Unknown record type";
	_recordTypeNames[BED3_RECORD_TYPE] = "Bed3 record type";
	_recordTypeNames[BED6_RECORD_TYPE] = "Bed6 record type";
	_recordTypeNames[BED12_RECORD_TYPE] = "Bed12 record type";
	_recordTypeNames[BED_PLUS_RECORD_TYPE] = "BedPlus record type";
	_recordTypeNames[BAM_RECORD_TYPE] = "BAM record type";
	_recordTypeNames[VCF_RECORD_TYPE] = "VCF record type";
	_recordTypeNames[GFF_RECORD_TYPE] = "GFF record type";

//	UNKNOWN_FILE_TYPE, SINGLE_LINE_DELIM_TEXT_FILE_TYPE,
//				MULTI_LINE_ENTRY_TEXT_FILE_TYPE,
//				GFF_FILE_TYPE, GZIP_FILE_TYPE, BAM_FILE_TYPE

	_fileTypeNames[UNKNOWN_FILE_TYPE] = "Unknown file type";
	_fileTypeNames[SINGLE_LINE_DELIM_TEXT_FILE_TYPE] = "Delimited text file type";
	_fileTypeNames[GZIP_FILE_TYPE] = "Gzip file type";
	_fileTypeNames[BAM_FILE_TYPE] = "BAM file type";
	_fileTypeNames[VCF_FILE_TYPE] = "VCF file type";
}


bool FileRecordTypeChecker::scanBuffer(const char *buffer, size_t len)
{
	if (len == 0) {
		len = strlen(buffer);
	}
	_numBytesInBuffer = len;
	if (_numBytesInBuffer == 0) {
		cerr << "Error: " << _filename << " is an empty file."<< endl;
		exit(1);
	}

	//special: the first thing we do is look for a gzipped file.
	if (!_isGzipped && ((unsigned char)(buffer[0]) == 0x1f)) {
		_isGzipped = true;
		return true;
	}
	//scan the first 8K block of the streamBuf.

	//now we have a buffer from the file.
	//first, test to see if it's binary or text.
	if (isBinaryBuffer(buffer, len)) {
		_isText = false;
		_isBinary = true;
		return true;
	} else {
		_isText = true;
		_isBinary = false;
		return handleTextFormat(buffer, len);
	}
}

bool FileRecordTypeChecker::isBinaryBuffer(const char *buffer, size_t len)
{
	if (isBAM(buffer)) {
		return true;
	}

	//Let's say that in a text file, at least 90% of the characters
	//should be alphanumeric, whitespace, or punctuation.

	int alphaNumCount = 0;
	int whiteSpaceCount = 0;
	int punctuationCount = 0;

	for (int i=0; i < (int)len; i++) {
		char currChar = buffer[i];
		if (isalnum(currChar)) {
			alphaNumCount++;
		} else if (isspace(currChar)) {
			whiteSpaceCount++;
		} else if (ispunct(currChar)) {
			punctuationCount++;
		}
	}

	if ((float)(alphaNumCount + whiteSpaceCount + punctuationCount) / (float)(_numBytesInBuffer) < PERCENTAGE_PRINTABLE) {
		return true;
	}
	return false;
}


bool FileRecordTypeChecker::isBAM(const char *buffer)
{
	//check for BAM. The Bam Magic String is "BAM\1", and should be the first 4 characters of the file.

	if (strncmp(buffer, "BAM\1", 4) == 0) {
		_isBAM = true;
		_fileType = BAM_FILE_TYPE;
		_recordType = BAM_RECORD_TYPE;
		return true;
	}

	//TBD: Handle other binary formats
	return false;
}

bool FileRecordTypeChecker::handleTextFormat(const char *buffer, size_t len)
{
	if (isVCFformat(buffer)) {
		return isTextDelimtedFormat(buffer, len);
	} else if (isTextDelimtedFormat(buffer, len)) {

		//At this point, _isText and _isDelimited are set. _numFields and _delimChar are
		//set.
		_fileType = SINGLE_LINE_DELIM_TEXT_FILE_TYPE;

		//Tokenize the first line of valid data into fields.
		const QuickString &line = _lines[_firstValidDataLineIdx];
		_currLineElems.clear();
		if (Tokenize(line, _currLineElems, _delimChar, _numFields) != _numFields) {
			cerr << "Error: Type checker found wrong number of fields while tokenizing data line." << endl;
			exit(1);
		}

		if (isBedFormat()) {
			_isBed = true;
			if (_numFields == 3) {
				_recordType = BED3_RECORD_TYPE;
			} else if (_numFields == 4) {
				if (isNumeric(_currLineElems[3])) {
					_recordType = BEDGRAPH_RECORD_TYPE;
					_fourthFieldNumeric = true;
				} else {
					_fourthFieldNumeric = false;
					_recordType = BED4_RECORD_TYPE;
				}
			} else if (_numFields == 5) {
				_recordType = BED5_RECORD_TYPE;
			} else if (_numFields == 6) {
				_recordType = BED6_RECORD_TYPE;
			} else if (_numFields == 12) {
				_recordType = BED12_RECORD_TYPE;
			} else if (_numFields >6) {
				_recordType = BED_PLUS_RECORD_TYPE;
			}
			return true;
		}
		if (isGFFformat()) {
			_isGFF = true;
			_recordType = GFF_RECORD_TYPE;
			return true;
		}
		return false;
	}
	return false;
}

bool FileRecordTypeChecker::isVCFformat(const char *buffer)
{
	if (_isVCF) {
		return true; //previous pass through this method has determined file is VCF.
	}
	if (memcmp(buffer, "##fileformat=VCF", 16) == 0) {
		_isVCF = true;
		_fileType = VCF_FILE_TYPE;
		_recordType = VCF_RECORD_TYPE;
		return true;
	}
	return false;
}

bool FileRecordTypeChecker::isBedFormat() {

	//test that the file has at least three fields.
	//2nd and 3rd fields of first valid data line must be integers. 3rd must not be less than 2nd.
	if (_numFields < 3) {
		return false;
	}
	//the 2nd and 3rd fields must be numeric.
	if (!isNumeric(_currLineElems[1]) || !isNumeric(_currLineElems[2])) {
		return false;
	}

	int start = str2chrPos(_currLineElems[1]);
	int end = str2chrPos(_currLineElems[2]);
	if (end < start) {
		return false;
	}
	return true;
}

bool FileRecordTypeChecker::isGFFformat()
{
	//a GFF file may have 8 or 9 fields.
	if (_numFields < 7 || _numFields > 9) {
		return false;
	}
	//the 4th and 5th fields must be numeric.
	if (!isNumeric(_currLineElems[3]) || !isNumeric(_currLineElems[4])) {
		return false;
	}
	int start = str2chrPos(_currLineElems[3]);
	int end = str2chrPos(_currLineElems[4]);
	if (end < start) {
		return false;
	}
	return true;
}

bool FileRecordTypeChecker::isTextDelimtedFormat(const char *buffer, size_t len)
{
	//Break single string buffer into vector of QuickStrings. Delimiter is newline.
	_lines.clear();
	int numLines = Tokenize(buffer, _lines, '\n', len);

	//anticipated delimiter characters are tab, comma, and semi-colon.
	//If we need new ones, they must be added in this method.
	//search each line for delimiter characters.

	vector<int> tabCounts;
	vector<int> commaCounts;
	vector<int> semicolonCounts;

	tabCounts.reserve(numLines);
	commaCounts.reserve(numLines);
	semicolonCounts.reserve(numLines);

	//loop through the lines, ignoring headers. Count potential delimiters,
	//see if we can find a few lines with the same number of a given delimiter.
	//delims are tested in hierarchical order, starting with tab,then comma, then semi-colon.

	int validLinesFound=0;
	int headerCount = 0;
	int emptyLines = 0;
	for (int i=0; i < numLines; i++ ) {

		if (validLinesFound >=4) {
			break; //really only need to look at like 4 lines of data, max.
		}
		QuickString &line = _lines[i];
		int len =line.size();
		//skip over any empty line
		if (len == 0) {
			emptyLines++;
			continue;
		}

		//skip over any header line
		if (isHeaderLine(line)) {
			//clear any previously found supposedly valid data lines, because valid lines can only come after header lines.
			if (_firstValidDataLineIdx > -1 && _firstValidDataLineIdx < i) {
				_firstValidDataLineIdx = -1;
				validLinesFound--;
				headerCount++;
			}
			headerCount++;
			continue;
		}
		//a line must have some alphanumeric characters in order to be valid.
		bool hasAlphaNum = false;
		for (int j=0; j < len; j++) {
			if(isalnum(line[j])) {
				hasAlphaNum = true;
				break;
			}
		}
		if (!hasAlphaNum) {
			continue;
		}

		validLinesFound++;
		if (_firstValidDataLineIdx == -1) {
			_firstValidDataLineIdx = i;
		}

		int lineTabCount = 0, lineCommaCount=0, lineSemicolonCount =0;
		for (int j=0; j < len; j++) {
			char currChar = line[j];
			if (currChar == '\t') {
				lineTabCount++;
			} else if (currChar == ',') {
				lineCommaCount++;
			} else if (currChar == ';') {
				lineSemicolonCount++;
			}
		}
		tabCounts.push_back(lineTabCount);
		commaCounts.push_back(lineCommaCount);
		semicolonCounts.push_back(lineSemicolonCount);
	}

	if (headerCount + emptyLines == numLines) {
		_insufficientData = true;
	}
	if (validLinesFound == 0) {
		return false;
	}
	_insufficientData = false;

	if (delimiterTesting(tabCounts, '\t')) {
		return true;
	}
	if (delimiterTesting(commaCounts, ',')) {
		return true;
	}
	if (delimiterTesting(semicolonCounts, ';')) {
		return true;
	}

	return false; //unable to detect delimited file.
}

bool FileRecordTypeChecker::delimiterTesting(vector<int> &counts, char suspectChar)
{
	//check to see if we found the same number of tabs in every line.
	int numDelims = counts[0];
	if (numDelims != 0) {
		bool countsMatch = true;
		for (int i=1;  i < (int)counts.size(); i++) {
			if (counts[i] != numDelims) {
				countsMatch = false;
			}
		}
		if (countsMatch) {
			//Hurray!! We have successfully found a delimited file.
			_isDelimited = true;
			_delimChar = suspectChar;
			_numFields = numDelims +1;
			return true;
		} else {
			return false;
		}
	}
	return false;
}


void FileRecordTypeChecker::setBam()
{
	_fileType = BAM_FILE_TYPE;
	_recordType = BAM_RECORD_TYPE;
	_isBinary = true;
	_isBAM = true;
}
