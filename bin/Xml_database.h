#ifndef XML_DATABASE_H
#define XML_DATABASE_H

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>

struct XMLField {
    std::string name;
    std::string value;
};

struct XMLRow {
    std::string pk_value;
    std::vector<XMLField> fields;
};

struct XMLTable {
    std::string tableName;
    std::string primaryKey;
    std::vector<XMLRow> rows;
};

class XMLDatabase {
private:
    std::string filePath;
    std::vector<XMLTable> tables;

    std::string extract(std::string str, std::string startTag, std::string endTag) {
        size_t start = str.find(startTag);
        if (start == std::string::npos) return "";
        start += startTag.length();
        size_t end = str.find(endTag, start);
        if (end == std::string::npos) return "";
        return str.substr(start, end - start);
    }

public:
    XMLDatabase(std::string path) : filePath(path) {
        load();
    }

    void createTable(std::string name, std::string pk) {
        for (auto& t : tables) if (t.tableName == name) return;
        XMLTable newTable;
        newTable.tableName = name;
        newTable.primaryKey = pk;
        tables.push_back(newTable);
        save();
    }

    // Insert only: no-op if the primary key already exists.
    bool insert(std::string tableName, std::string pk_val, std::vector<XMLField> data) {
        for (auto& t : tables) {
            if (t.tableName == tableName) {
                for (auto& r : t.rows)
                    if (r.pk_value == pk_val) return false;
                XMLRow newRow;
                newRow.pk_value = pk_val;
                newRow.fields   = data;
                t.rows.push_back(newRow);
                save();
                return true;
            }
        }
        return false;
    }

    // Overwrite the fields of an existing row.
    bool update(std::string tableName, std::string pk_val, std::vector<XMLField> data) {
        for (auto& t : tables) {
            if (t.tableName == tableName) {
                for (auto& r : t.rows) {
                    if (r.pk_value == pk_val) {
                        r.fields = data;
                        save();
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // ── NEW: Delete a single row by primary key ───────────────────────────
    // Returns true if the row was found and removed, false otherwise.
    bool deleteRow(std::string tableName, std::string pk_val) {
        for (auto& t : tables) {
            if (t.tableName == tableName) {
                auto& rows = t.rows;
                for (auto it = rows.begin(); it != rows.end(); ++it) {
                    if (it->pk_value == pk_val) {
                        rows.erase(it);
                        save();
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // ── NEW: Delete all rows where a named field matches any value in the
    //         given list.  Returns the number of rows deleted.
    //
    //  Example — delete every Complete / Incomplete / Ignore task while
    //  leaving Expired rows untouched:
    //
    //    db.deleteWhere("Tasks", "status",
    //                   {"Complete", "Incomplete", "Ignore",
    //                    "Active", "Upcoming", "Started"});
    //
    //  NOTE: "status" here means the *stored* value in the XML, which is
    //  always one of {Incomplete, Complete, Ignore} because Active /
    //  Upcoming / Started / Expired are derived at render time.  Pass the
    //  stored values you actually want to erase.
    int deleteWhere(std::string tableName,
                    std::string fieldName,
                    std::vector<std::string> matchValues)
    {
        int removed = 0;
        for (auto& t : tables) {
            if (t.tableName != tableName) continue;
            auto& rows = t.rows;
            auto it = rows.begin();
            while (it != rows.end()) {
                std::string val = "";
                for (const auto& f : it->fields)
                    if (f.name == fieldName) { val = f.value; break; }
                bool match = false;
                for (const auto& mv : matchValues)
                    if (val == mv) { match = true; break; }
                if (match) {
                    it = rows.erase(it);
                    ++removed;
                } else {
                    ++it;
                }
            }
        }
        if (removed > 0) save();
        return removed;
    }

    void load() {
        std::ifstream file(filePath);
        if (!file.is_open()) return;

        std::string line, content;
        while (std::getline(file, line)) content += line;
        file.close();

        tables.clear();
        size_t tablePos = content.find("<table");
        while (tablePos != std::string::npos) {
            XMLTable t;
            size_t tagEnd = content.find(">", tablePos);
            std::string tableTag = content.substr(tablePos, tagEnd - tablePos);

            t.tableName  = extract(tableTag, "name=\"", "\"");
            t.primaryKey = extract(tableTag, "pk=\"",   "\"");

            size_t endTablePos  = content.find("</table>", tablePos);
            std::string tableContent = content.substr(tagEnd + 1, endTablePos - (tagEnd + 1));

            size_t rowPos = tableContent.find("<row");
            while (rowPos != std::string::npos) {
                XMLRow r;
                size_t rowTagEnd = tableContent.find(">", rowPos);
                std::string rowTag = tableContent.substr(rowPos, rowTagEnd - rowPos);
                r.pk_value = extract(rowTag, t.primaryKey + "=\"", "\"");

                size_t endRowPos = tableContent.find("</row>", rowPos);
                std::string rowContent = tableContent.substr(rowTagEnd + 1,
                                                             endRowPos - (rowTagEnd + 1));

                size_t fieldPos = rowContent.find("<");
                while (fieldPos != std::string::npos && fieldPos < rowContent.length()) {
                    if (rowContent[fieldPos + 1] == '/') {
                        fieldPos = rowContent.find("<", fieldPos + 1);
                        continue;
                    }
                    size_t fieldNameEnd = rowContent.find(">", fieldPos);
                    std::string fName = rowContent.substr(fieldPos + 1, fieldNameEnd - fieldPos - 1);
                    std::string fVal  = extract(rowContent, "<" + fName + ">", "</" + fName + ">");
                    r.fields.push_back({fName, fVal});
                    fieldPos = rowContent.find("<", rowContent.find("</" + fName + ">", fieldNameEnd));
                }

                t.rows.push_back(r);
                rowPos = tableContent.find("<row", endRowPos);
            }
            tables.push_back(t);
            tablePos = content.find("<table", endTablePos);
        }
    }

    void save() {
        std::ofstream file(filePath, std::ios::trunc);
        file << "<?xml version=\"1.0\"?>\n<database>\n";
        for (const auto& t : tables) {
            file << "  <table name=\"" << t.tableName << "\" pk=\"" << t.primaryKey << "\">\n";
            for (const auto& r : t.rows) {
                file << "    <row " << t.primaryKey << "=\"" << r.pk_value << "\">\n";
                for (const auto& f : r.fields)
                    file << "      <" << f.name << ">" << f.value << "</" << f.name << ">\n";
                file << "    </row>\n";
            }
            file << "  </table>\n";
        }
        file << "</database>";
        file.close();
    }

    std::vector<XMLRow> selectAll(std::string tableName) {
        for (auto& t : tables) if (t.tableName == tableName) return t.rows;
        return {};
    }
};

#endif