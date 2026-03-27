#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <iomanip>
#include "libs/json.hpp" // Ensure you have this header-only library

using json = nlohmann::json;
using namespace std;

enum SemesterType { FALL, SPRING, SUMMER };

struct Course {
    string id;
    string name;
    int credits;
    set<string> offered; // "Fall", "Spring", "Summer"
    set<string> prereqs;
    set<string> concurrency; // Prereq with concurrency
};

struct Curriculum {
    string major;
    map<string, Course> courseMap;
};

struct Student {
    string name;
    string major;
    int currentSemesterIdx = 0;
    set<string> completedCourses;
    vector<vector<string>> schedule;

    bool isFinished(const Curriculum& cur) {
        for (auto const& [id, course] : cur.courseMap) {
            if (completedCourses.find(id) == completedCourses.end()) return false;
        }
        return true;
    }
};

// Logic to load JSON into our C++ structures
Curriculum loadCurriculum(string filename) {
    ifstream file(filename);
    json j;
    file >> j;

    Curriculum cur;
    cur.major = j["major"];
    for (auto& item : j["courses"]) {
        Course c;
        c.id = item["id"];
        c.name = item["name"];
        c.credits = item["credits"];
        for (string s : item["offered"]) c.offered.insert(s);
        for (string p : item["prerequisites"]) c.prereqs.insert(p);
        for (string co : item["prereq_concurrency"]) c.concurrency.insert(co);
        cur.courseMap[c.id] = c;
    }
    return cur;
}

class Scheduler {
public:
    void generateSchedules(Curriculum& ae, Curriculum& ee, Student& s1, Student& s2) {
        int maxSemesters = 16; 
        int creditLimit = 18;

        while ((!s1.isFinished(ae) || !s2.isFinished(ee)) && s1.currentSemesterIdx < maxSemesters) {
            s1.schedule.push_back({});
            s2.schedule.push_back({});

            string termName = (s1.currentSemesterIdx % 2 == 0) ? "Fall" : "Spring";

            // Find what's available for each student this term
            set<string> eligible1 = getEligible(ae, s1, termName);
            set<string> eligible2 = getEligible(ee, s2, termName);

            // Find the Overlap (Common Classes)
            set<string> common;
            for (const string& id : eligible1) {
                if (eligible2.count(id)) common.insert(id);
            }

            int credits1 = 0, credits2 = 0;

            // 1. Prioritize Common Classes to maximize overlap
            for (const string& id : common) {
                if (credits1 + ae.courseMap[id].credits <= creditLimit && 
                    credits2 + ee.courseMap[id].credits <= creditLimit) {
                    s1.schedule.back().push_back(id);
                    s2.schedule.back().push_back(id);
                    credits1 += ae.courseMap[id].credits;
                    credits2 += ee.courseMap[id].credits;
                }
            }

            // 2. Fill remaining slots for Student 1 (Major Specific)
            for (const string& id : eligible1) {
                if (find(s1.schedule.back().begin(), s1.schedule.back().end(), id) != s1.schedule.back().end()) continue;
                if (credits1 + ae.courseMap[id].credits <= creditLimit) {
                    s1.schedule.back().push_back(id);
                    credits1 += ae.courseMap[id].credits;
                }
            }

            // 3. Fill remaining slots for Student 2 (Major Specific)
            for (const string& id : eligible2) {
                if (find(s2.schedule.back().begin(), s2.schedule.back().end(), id) != s2.schedule.back().end()) continue;
                if (credits2 + ee.courseMap[id].credits <= creditLimit) {
                    s2.schedule.back().push_back(id);
                    credits2 += ee.courseMap[id].credits;
                }
            }

            // Mark courses as completed for next iteration
            for (string id : s1.schedule.back()) s1.completedCourses.insert(id);
            for (string id : s2.schedule.back()) s2.completedCourses.insert(id);
            
            s1.currentSemesterIdx++;
            s2.currentSemesterIdx++;
        }
    }

private:
    set<string> getEligible(const Curriculum& cur, const Student& s, string term) {
        set<string> eligible;
        for (auto const& [id, course] : cur.courseMap) {
            if (s.completedCourses.count(id)) continue;
            
            // Check if course is offered this semester
            if (course.offered.find(term) == course.offered.end()) continue;

            // Check Hard Prereqs (must be done)
            bool prereqsMet = true;
            for (const string& p : course.prereqs) {
                if (s.completedCourses.find(p) == s.completedCourses.end()) {
                    prereqsMet = false; break;
                }
            }
            if (!prereqsMet) continue;

            // Note: Concurrency logic would check if the concurrency course is also in 'eligible'
            eligible.insert(id);
        }
        return eligible;
    }
};

int main() {
    try {
        Curriculum ae = loadCurriculum("course_data/UAH_BSAE.json");
        Curriculum ee = loadCurriculum("course_data/UAH_BSEE.json");

        Student s1 = {"Alice", "Aerospace"};
        Student s2 = {"Bob", "Electrical"};

        Scheduler scheduler;
        scheduler.generateSchedules(ae, ee, s1, s2);

        cout << left << setw(15) << "Term" << setw(25) << "Alice (AE)" << " | " << "Bob (EE)" << endl;
        cout << string(70, '-') << endl;

        for (int i = 0; i < s1.schedule.size(); ++i) {
            string termLabel = "Sem " + to_string(i+1) + " (" + ((i%2==0)?"F":"S") + ")";
            size_t rows = max(s1.schedule[i].size(), s2.schedule[i].size());
            
            for (size_t r = 0; r < rows; ++r) {
                string c1 = (r < s1.schedule[i].size()) ? s1.schedule[i][r] : "";
                string c2 = (r < s2.schedule[i].size()) ? s2.schedule[i][r] : "";
                string shared = (c1 == c2 && !c1.empty()) ? " [SHARED]" : "";

                cout << left << setw(15) << (r == 0 ? termLabel : "") 
                     << setw(25) << c1 << " | " << c2 << shared << endl;
            }
            cout << endl;
        }
    } catch (exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
    return 0;
}