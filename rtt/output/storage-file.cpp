#include "storage-file.h"

namespace rtt {
namespace output {
namespace file {

const std::string Storage::XPATH_FILE_MAIN      = "TOOLKIT_SETTINGS/OUTPUT/FILE/MAIN_FILE";
const std::string Storage::XPATH_DIR_DH         = "TOOLKIT_SETTINGS/OUTPUT/FILE/DIEHARDER_DIR";
const std::string Storage::XPATH_DIR_NIST       = "TOOLKIT_SETTINGS/OUTPUT/FILE/NIST_STS_DIR";
const std::string Storage::XPATH_DIR_TU01_SC    = "TOOLKIT_SETTINGS/OUTPUT/FILE/TU01_SMALLCRUSH_DIR";
const std::string Storage::XPATH_DIR_TU01_C     = "TOOLKIT_SETTINGS/OUTPUT/FILE/TU01_CRUSH_DIR";
const std::string Storage::XPATH_DIR_TU01_BC    = "TOOLKIT_SETTINGS/OUTPUT/FILE/TU01_BIGCRUSH_DIR";
const std::string Storage::XPATH_DIR_TU01_RB    = "TOOLKIT_SETTINGS/OUTPUT/FILE/TU01_RABBIT_DIR";
const std::string Storage::XPATH_DIR_TU01_AB    = "TOOLKIT_SETTINGS/OUTPUT/FILE/TU01_ALPHABIT_DIR";

const size_t Storage::MISC_TAB_SIZE     = 4;
const size_t Storage::MISC_COL_WIDTH    = 30;

std::unique_ptr<Storage> Storage::getInstance(TiXmlNode * root ,
                                              const CliOptions & options ,
                                              const time_t & creationTime) {
    std::unique_ptr<Storage> storage (new Storage());
    storage->creationTime = creationTime;
    storage->batteryConstant = options.getBattery();
    storage->inFilePath = options.getBinFilePath();

    std::string dirXPath;
    switch(storage->batteryConstant) {
    case Constants::BATTERY_DIEHARDER       : dirXPath = XPATH_DIR_DH; break;
    case Constants::BATTERY_NIST_STS        : dirXPath = XPATH_DIR_NIST; break;
    case Constants::BATTERY_TU01_SMALLCRUSH : dirXPath = XPATH_DIR_TU01_SC; break;
    case Constants::BATTERY_TU01_CRUSH      : dirXPath = XPATH_DIR_TU01_C; break;
    case Constants::BATTERY_TU01_BIGCRUSH   : dirXPath = XPATH_DIR_TU01_BC; break;
    case Constants::BATTERY_TU01_RABBIT     : dirXPath = XPATH_DIR_TU01_RB; break;
    case Constants::BATTERY_TU01_ALPHABIT   : dirXPath = XPATH_DIR_TU01_AB; break;
    default:
        throw std::runtime_error("unknown battery constant: " +
                                 Utils::itostr(options.getBattery()));
    }

    /* Getting file name for main output file */
    storage->mainOutFilePath = getXMLElementValue(root , XPATH_FILE_MAIN);

    /* Creating file name for test report file */
    std::string binFileName = Utils::getLastItemInPath(storage->inFilePath);
    std::string datetime = Utils::formatRawTime(storage->creationTime , "%Y%m%d%H%M%S");
    storage->outFilePath = getXMLElementValue(root , dirXPath);
    if(!storage->outFilePath.empty() && storage->outFilePath.back() != '/')
        storage->outFilePath.append("/");
    storage->outFilePath.append(datetime + "-" + binFileName + ".report");

    storage->makeReportHeader();

    return storage;
}

void Storage::addNewTest(const std::string & testName) {
    report << "---------------------------------------------------" << std::endl;
    report << testName << " test results:" << std::endl;
    ++indent;
}

void Storage::setTestOptions(const std::vector<std::string> & options) {
    report << doIndent() << "Test settings: " << std::endl;
    ++indent;
    std::string tabs = doIndent();
    for(const std::string & i : options)
        report << tabs << i << std::endl;
    --indent;
    report << doIndent() << "************" << std::endl;
    report << std::endl;
}

void Storage::addSubTest() {
    ++currentSubtest;
    report << doIndent() << "Subtest " << currentSubtest << ":" << std::endl;
    ++indent;
}

void Storage::addStatisticResult(const std::string & statName, double value, int precision) {
    ++totalStatisticsCount;
    if(value >= Constants::MATH_ALPHA && value <= (1.0 - Constants::MATH_ALPHA))
        ++passedStatisticsCount;

    report << doIndent() << statName << " statistic p-value: "
           << std::setprecision(precision) << std::fixed << value << std::endl;
}

void Storage::addStatisticResult(const std::string & statName ,
                                 const std::string & value, bool failed) {
    ++totalStatisticsCount;
    if(!failed)
        ++passedStatisticsCount;

    report << doIndent() << statName << " statistic: " << value << std::endl;
}

void Storage::addPValues(const std::vector<double> & pvals , int precision) {
    report << doIndent() << "p-values: " << std::endl;
    ++indent;
    std::string tabs = doIndent();
    for(const double & i : pvals)
        report << tabs << std::setprecision(precision) << std::fixed << i << std::endl;
    --indent;
    report << doIndent() << "============" << std::endl;
}

void Storage::finalizeSubTest() {
    --indent;
    report << doIndent() << "############" << std::endl;
    report << std::endl;
}

void Storage::finalizeTest() {
    report << "---------------------------------------------------" << std::endl << std::endl;
    --indent;
    currentSubtest = 0;
}

void Storage::finalizeReport() {
    /* Add passed tests proportion at the end of report */
    passedTestProp = {Utils::itostr(passedStatisticsCount) + "/" + Utils::itostr(totalStatisticsCount)};
    report << "Proportion of passed statistics: " << passedTestProp << std::endl;
    /* Storing report */
    Utils::createDirectory(Utils::getPathWithoutLastItem(outFilePath));
    Utils::saveStringToFile(outFilePath , report.str());
    /* Adding result into table file */
    /* Files with same name as the file processed in
     * this run will be assigned new results */
    addResultToTableFile();
}

void Storage::makeReportHeader() {
    report << "***** Randomness Testing Toolkit file analysis report *****" << std::endl;
    report << "Date:    " << Utils::formatRawTime(creationTime , "%d-%m-%Y") << std::endl;
    report << "File:    " << inFilePath << std::endl;
    report << "Battery: " << Constants::batteryToString(batteryConstant) << std::endl;
    report << std::endl << std::endl;
}

std::string Storage::doIndent() const {
    if(indent > 0)
        return std::string(indent * MISC_TAB_SIZE, ' ');
    else
        return "";
}

void Storage::addResultToTableFile() const {
    if(Utils::fileExist(mainOutFilePath)) {
        /* Table file already exist */
        tStringVector header;
        tStringVector fileNames;
        std::vector<tStringVector> tableData;
        /* Loading file into table variables */
        loadMainTable(header , fileNames , tableData);

        /* Looking for file name - if table already has row
         * with same file name as I am processing here,
         * only new information is added to this row.
         * Otherwise new row is added. */
        std::vector<std::string>::iterator fileRow =
                std::find(fileNames.begin() , fileNames.end() , inFilePath);
        if(fileRow != fileNames.end()) {
            /* Row with same filename is present, modifying this row */
            int rowIndex = std::distance(fileNames.begin() , fileRow);
            tableData.at(rowIndex).at(0) =
                    Utils::formatRawTime(creationTime , "%Y-%m-%d %H:%M:%S");
            tableData.at(rowIndex).at(batteryConstant) = passedTestProp;
        } else {
            /* Adding new row */
            addNewRow(fileNames , tableData);
        }

        /* Saving table variables into file. */
        saveMainTable(header , fileNames , tableData);
    } else {
        /* File doesn't exist, create brand new table file with single new result */
        tStringVector header;
        tStringVector fileNames;
        std::vector<tStringVector> tableData;

        /* Creating header */
        header.push_back("Input file path");
        header.push_back("Time of last update");
        for(uint i = 1 ; i <= Constants::BATTERY_TOTAL_COUNT ; ++i)
            header.push_back(Constants::batteryToString(i));

        /* Adding new row */
        addNewRow(fileNames , tableData);

        /* Table variables are now complete, save into file */
        saveMainTable(header , fileNames , tableData);
    }
}

void Storage::loadMainTable(tStringVector & header,
                            tStringVector & fileNames,
                            std::vector<tStringVector> & tableData) const {
    std::string table = std::move(Utils::readFileToString(mainOutFilePath));
    tStringVector lines = Utils::split(table , '\n');
    if(lines.empty())
        throw std::runtime_error("can't load table: file is empty");

    /* Parsing header */
    header = std::move(Utils::split(lines.at(0) , '^'));

    /* Parsing rows */
    tStringVector row;
    for(size_t i = 1 ; i < lines.size() ; ++i) {
        row = std::move(Utils::split(lines.at(i) , '^'));
        if(row.size() != 1)
            throw std::runtime_error("input table corrupted: "
                                     "non header row contains more than one \"^\"");

        row = std::move(Utils::split(row.at(0) , '|'));
        if(row.size() != header.size())
            throw std::runtime_error("input table corrupted: "
                                     "row " + Utils::itostr(i) + " has "
                                     "different number of columns than header");
        fileNames.push_back(stripSpacesFromString(row.at(0)));
        tableData.push_back({row.begin() + 1 , row.end()});
    }
}

void Storage::saveMainTable(const tStringVector & header,
                            const tStringVector & fileNames,
                            const std::vector<tStringVector> & tableData) const {
    if(fileNames.size() != tableData.size())
        throw std::runtime_error("can't create output table: number of filenames and"
                                 " number of table data rows does not match");

    /* First columnt is aligned to left, rest right */
    bool first = true;
    std::stringstream table;

    /* Creating header */
    for(const std::string & val : header) {
        table << "^";
        table << std::setw(MISC_COL_WIDTH);
        if(first){
            table << std::left;
            first = false;
        }
        else /* This else is maybe useless */
            table << std::right;
        table << val;
    }
    table << "^" << std::endl;

    /* Inserting rows */
    for(size_t i = 0 ; i < fileNames.size() ; ++i) {
        table << "^";
        table << std::setw(MISC_COL_WIDTH) << std::left;
        table << fileNames.at(i);
        table << std::right;
        for(const std::string & val : tableData.at(i)) {
            table << "|";
            table << std::setw(MISC_COL_WIDTH);
            table << val;
        }
        table << "|" << std::endl;
    }

    /* Saving table to file */
    Utils::saveStringToFile(mainOutFilePath , table.str());
}

void Storage::addNewRow(tStringVector & fileNames,
                        std::vector<tStringVector> & tableData) const {
    /* Add file path to file names column */
    fileNames.push_back(inFilePath);
    /* Add data into corresponding row */
    /* Column count is total batteries count + first column for last update info */
    tStringVector row(1 + Constants::BATTERY_TOTAL_COUNT , "");
    row.at(0) = Utils::formatRawTime(creationTime , "%Y-%m-%d %H:%M:%S");
    row.at(batteryConstant) = passedTestProp;
    tableData.push_back(std::move(row));
}

std::string Storage::stripSpacesFromString(const std::string & str) {
    size_t start = str.find_first_not_of(" ");
    size_t end = str.find_last_not_of(" ");
    return str.substr(start , end + 1);
}

} // namespace file
} // namespace output
} // namespace rtt
