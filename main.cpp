#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <iomanip>
#include "libs/json.hpp" 

using json = nlohmann::json;
using namespace std;

// --- Data Structures ---

struct Course {
    string id;
    string name;
    int credits;
    set<string> offered;
    set<string> prereqs;
    set<string> concurrency;
};

struct Curriculum {
    string major;
    map<string, Course> courseMap;
};

struct Student {
    string name;
    string major;
    int startSemesterOffset; 
    int targetCredits;       
    int targetGraduationTerm; // New: e.g., 8 for a standard 4-year graduation
    set<string> completedCourses;
    int currentTermIdx = 0;
    vector<vector<string>> schedule;

    int getRemainingCredits(const Curriculum& cur) const {
        int total = 0;
        for (auto const& [id, course] : cur.courseMap) {
            if (completedCourses.find(id) == completedCourses.end()) {
                total += course.credits;
            }
        }
        return total;
    }

    bool isFinished(const Curriculum& cur) {
        return getRemainingCredits(cur) == 0;
    }
};

// --- Function Prototypes ---

Curriculum loadCurriculum(string filename);
set<string> getEligible(const Curriculum& cur, const Student& s, string term);
bool canTakeWithConcurrency(const Curriculum& cur, const Student& s, string courseId);
void finalizeTerm(vector<Student>& students);

// --- Scheduler Class ---

class Scheduler {
public:
    void generateSchedules(map<string, Curriculum>& curricula, vector<Student>& students) {
        int maxTerms = 24; 
        bool allFinished = false;

        while (!allFinished && maxTerms > 0) {
            allFinished = true;
            map<int, set<string>> termEligible;
            
            for (int i = 0; i < (int)students.size(); ++i) {
                if (!students[i].isFinished(curricula[students[i].major])) {
                    allFinished = false;
                    string termName = ((students[i].currentTermIdx + students[i].startSemesterOffset) % 2 == 0) ? "Fall" : "Spring";
                    termEligible[i] = getEligible(curricula[students[i].major], students[i], termName);
                    students[i].schedule.push_back({});
                }
            }

            if (allFinished) break;

            map<string, int> courseFrequency;
            for (auto const& [idx, courses] : termEligible) {
                for (const string& id : courses) courseFrequency[id]++;
            }

            vector<pair<string, int>> sortedCommon(courseFrequency.begin(), courseFrequency.end());
            sort(sortedCommon.begin(), sortedCommon.end(), [](auto& a, auto& b) {
                return a.second > b.second; 
            });

            vector<int> currentCredits(students.size(), 0);
            
            // Allocation Phase with Graduation Urgency
            for (auto const& [courseId, freq] : sortedCommon) {
                for (int i = 0; i < (int)students.size(); ++i) {
                    if (termEligible[i].count(courseId)) {
                        auto& cur = curricula[students[i].major];
                        int courseCredits = cur.courseMap[courseId].credits;
                        
                        // Calculate "Urgency"
                        int remainingTerms = students[i].targetGraduationTerm - students[i].currentTermIdx;
                        int remainingCredits = students[i].getRemainingCredits(cur);
                        
                        // If they need more credits than their target allows to finish on time, 
                        // the effective target is pushed upward (Max 21 for safety).
                        int requiredAvg = (remainingTerms > 0) ? (remainingCredits / remainingTerms) : 21;
                        int effectiveTarget = max(students[i].targetCredits, requiredAvg);
                        effectiveTarget = min(effectiveTarget, 21); 

                        bool highCommonality = (freq > 1);
                        bool withinTarget = (currentCredits[i] + courseCredits <= effectiveTarget + 1);
                        bool catchUpPush = (highCommonality && currentCredits[i] < 12);

                        if ((withinTarget || catchUpPush) && 
                            canTakeWithConcurrency(cur, students[i], courseId)) {
                            
                            students[i].schedule.back().push_back(courseId);
                            currentCredits[i] += courseCredits;
                        }
                    }
                }
            }

            finalizeTerm(students);
            maxTerms--;
        }
    }
};

// --- Implementations ---

Curriculum loadCurriculum(string filename) {
    ifstream file(filename);
    if (!file.is_open()) throw runtime_error("Could not open " + filename);
    json j; file >> j;
    Curriculum cur;
    cur.major = j["major"];
    for (auto& item : j["courses"]) {
        Course c;
        c.id = item["id"];
        c.name = item.value("name", "");
        c.credits = item.value("credits", 3);
        for (string s : item["offered"]) c.offered.insert(s);
        for (string p : item["prerequisites"]) c.prereqs.insert(p);
        for (string co : item["prereq_concurrency"]) c.concurrency.insert(co);
        cur.courseMap[c.id] = c;
    }
    return cur;
}

set<string> getEligible(const Curriculum& cur, const Student& s, string term) {
    set<string> eligible;
    for (auto const& [id, course] : cur.courseMap) {
        if (s.completedCourses.count(id)) continue;
        if (course.offered.find(term) == course.offered.end()) continue;
        bool prereqsMet = true;
        for (const string& p : course.prereqs) {
            if (s.completedCourses.find(p) == s.completedCourses.end()) {
                prereqsMet = false; break;
            }
        }
        if (prereqsMet) eligible.insert(id);
    }
    return eligible;
}

bool canTakeWithConcurrency(const Curriculum& cur, const Student& s, string courseId) {
    const Course& c = cur.courseMap.at(courseId);
    for (const string& coreq : c.concurrency) {
        if (s.completedCourses.count(coreq)) continue;
        bool takingNow = false;
        for (const string& current : s.schedule.back()) {
            if (current == coreq) { takingNow = true; break; }
        }
        if (!takingNow) return false;
    }
    return true;
}

void finalizeTerm(vector<Student>& students) {
    for (auto& s : students) {
        if (s.schedule.size() > (size_t)s.currentTermIdx) {
            for (const string& id : s.schedule.back()) s.completedCourses.insert(id);
            s.currentTermIdx++;
        }
    }
}

int main() {
    try {
        map<string, Curriculum> curricula;
        curricula["Aerospace"] = loadCurriculum("course_data/UAH_BSAE.json");
        curricula["Electrical"] = loadCurriculum("course_data/UAH_BSEE.json");

        // Alice: Dual Enrollment, wants to graduate in 6 semesters (Accelerated)
        Student alice = {"Alice", "Aerospace", 0, 15, 6};
        alice.completedCourses = {"MA 171", "EH 101", "CH 121/125"}; 

        // Bob: AP credits, standard 8-semester goal
        Student bob = {"Bob", "Electrical", 0, 12, 8};
        bob.completedCourses = {"MA 171", "MA 172"}; 

        // Charlie: Only Calc 1, wants standard 8 semesters
        Student charlie = {"Charlie", "Aerospace", 0, 16, 8};
        charlie.completedCourses = {"MA 171"}; 

        // Damian: No credits, wants to graduate in 8 semesters
        Student damian = {"Damian", "Electrical", 0, 15, 8};
        damian.completedCourses = {}; 

        vector<Student> students = {alice, bob, charlie, damian};
        Scheduler scheduler;
        scheduler.generateSchedules(curricula, students);

        cout << left << setw(10) << "Term" << " | " << "Student (Hrs)" << " | " << "Schedule" << endl;
        cout << string(85, '-') << endl;

        for (int t = 0; t < 12; ++t) {
            bool termShown = false;
            for (auto& s : students) {
                if (t < (int)s.schedule.size() && !s.schedule[t].empty()) {
                    int hrs = 0;
                    for(auto& cid : s.schedule[t]) hrs += curricula[s.major].courseMap[cid].credits;
                    string termLabel = (!termShown) ? "Sem " + to_string(t+1) : "";
                    cout << left << setw(10) << termLabel << " | " << left << setw(15) << s.name + " (" + to_string(hrs) + ")" << " | ";
                    for (size_t i = 0; i < s.schedule[t].size(); ++i)
                        cout << s.schedule[t][i] << (i == s.schedule[t].size() - 1 ? "" : ", ");
                    cout << endl;
                    termShown = true;
                }
            }
            if (termShown) cout << endl;
        }
    } catch (exception& e) { cerr << e.what() << endl; return 1; }
    return 0;
}