#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "disk.hpp"

using namespace std;

struct record {
    int id;
    char fn[16];
    char ln[16];
    char ssn[16];
    char un[16];
    char pw[32];
};

const int RECORDS_PER_BLOCK = BLOCK_SIZE / sizeof(record);
const int DEGREE = 200;
const char DATA_NAME[] = "SampleRecordssorted.csv";

struct node {
    bool leaf;
    int next;
    int size;
    int key[DEGREE];
    int pageId[DEGREE + 1];
};

struct {
    int rootId;
    int nextRecord;
    int nextPage;
    int depth;
} s;

void initSuper() {
    s.rootId = -1;
    s.nextRecord = 0;
    s.nextPage = DISK_BLOCKS - 2;
    s.depth = 0;
}

void readSuper() {
    char temp[BLOCK_SIZE];
    block_read(DISK_BLOCKS - 1, temp);
    memcpy(&s, temp, sizeof(s));
}

void writeSuper() {
    char temp[BLOCK_SIZE];
    memcpy(temp, &s, sizeof(s));
    block_write(DISK_BLOCKS - 1, temp);
}

int addRecord(record r) {
    char temp[BLOCK_SIZE];
    block_read(s.nextRecord / RECORDS_PER_BLOCK, temp);
    memcpy(temp + s.nextRecord % RECORDS_PER_BLOCK * sizeof(record), &r, sizeof(record));
    block_write(s.nextRecord / RECORDS_PER_BLOCK, temp);
    s.nextRecord++;
    return s.nextRecord - 1;
}

node readNode(int page) {
    char temp[BLOCK_SIZE];
    node ret;
    block_read(page, temp);
    memcpy(&ret, temp, sizeof(node));
    return ret;
}

void writeNode(int page, node buf) {
    char temp[BLOCK_SIZE];
    memcpy(temp, &buf, sizeof(node));
    block_write(page, temp);
}

int getLeafNode(int nId, int val) {
    node n = readNode(nId);
    if(n.leaf) return nId;
    int i;
    for(i = 0; i < n.size; i++)
        if(n.key[i] > val) break;
    return getLeafNode(n.pageId[i], val);
}

int getParent(int childId) {
    if(childId == s.rootId) return -1;
    node child = readNode(childId);
    int parentId = s.rootId;
    node parent = readNode(s.rootId);
    int i, temp;
    while(1) {
        for(i = 0; i < parent.size; i++)
            if(parent.key[i] > child.key[0]) break;
        temp = parent.pageId[i];
        if(temp == childId) return parentId;
        parentId = temp;
        parent = readNode(parentId);
    };
}

bool search(int k, record *r) {
    node n = readNode(getLeafNode(s.rootId, k));
    for(int i = 0; i < n.size; i++) {
        if(n.key[i] == k) {
            char temp[BLOCK_SIZE];
            block_read(n.pageId[i] / RECORDS_PER_BLOCK, temp);
            memcpy(r, temp + n.pageId[i] % RECORDS_PER_BLOCK * sizeof(record), sizeof(record));
            return true;
        }
    }
    cout << "record not found\n";
    return false;
}

void insertNonLeaf(int nId, int k2, int n2Id) {
    if(nId == s.rootId) {
        s.rootId = s.nextPage;
        s.nextPage--;
        node root;
        root.leaf = false;
        root.next = -1;
        root.pageId[0] = nId;
        root.pageId[1] = n2Id;
        root.key[0] = k2;
        root.size = 1;
        writeNode(s.rootId, root);
        s.depth++;
        return;
    }
    int pId = getParent(nId);
    node p = readNode(pId);
    int i;
    for(i = 0; i < p.size; i++)
        if(p.key[i] > k2) break;
    int tempK, tempP;
    for(int j = i; j < p.size; j++) {
        tempK = p.key[j];
        p.key[j] = k2;
        k2 = tempK;
        tempP = p.pageId[j + 1];
        p.pageId[j + 1] = n2Id;
        n2Id = tempP;
    }
    p.key[p.size] = k2;
    p.size++;
    p.pageId[p.size] = n2Id;
    if(p.size < DEGREE) {
        writeNode(pId, p);
        return;
    }
    int p2Id = s.nextPage;
    s.nextPage--;
    node p2;
    p2.leaf = false;
    p2.next = -1;
    p2.size = 0;
    int newSize = (DEGREE + 1) / 2;
    int k3 = p.key[newSize];
    for(i = newSize + 1; i < DEGREE; i++) {
        p2.pageId[p2.size] = p.pageId[i];
        p2.key[p2.size] = p.key[i];
        p2.size++;
    }
    p2.pageId[p2.size] = p.pageId[i];
    p.size = newSize;
    writeNode(pId, p);
    writeNode(p2Id, p2);
    insertNonLeaf(pId, k3, p2Id);
}

void insert(record rec) {
    int k = rec.id;
    int r = addRecord(rec);
    int nId;
    node n;
    if(s.rootId == -1) {
        s.rootId = s.nextPage;
        s.nextPage--;
        node root;
        root.leaf = true;
        root.next = -1;
        root.size = 0;
        s.depth++;
        nId = s.rootId;
        n = root;
    } else {
        nId = getLeafNode(s.rootId, k);
        n = readNode(nId);
    }
    int i;
    for(i = 0; i < n.size; i++)
        if(k < n.key[i]) break;
    int tempK, tempR;
    for(int j = i; j < n.size; j++){
        tempK = n.key[j];
        n.key[j] = k;
        k = tempK;
        tempR = n.pageId[j];
        n.pageId[j] = r;
        r = tempR;
    }
    n.key[n.size] = k;
    n.pageId[n.size] = r;
    n.size++;
    if(n.size < DEGREE) {
        writeNode(nId, n);
        return;
    }
    int n2Id = s.nextPage;
    s.nextPage--;
    node n2;
    n2.leaf = true;
    n2.size = 0;
    n2.next = n.next;
    n.next = n2Id;
    int newSize = (DEGREE + 1) / 2;
    for(int i = newSize; i < DEGREE; i++) {
        n2.key[n2.size] = n.key[i];
        n2.pageId[n2.size] = n.pageId[i];
        n2.size++;
    }
    n.size = newSize;
    writeNode(nId, n);
    writeNode(n2Id, n2);
    insertNonLeaf(nId, n2.key[0], n2Id);
}

void readRecords() {
    fstream fin;
    fin.open(DATA_NAME, ios::in);

    vector<string> row;
    string line, word;
    record temp;

    getline(fin, line);
    while(getline(fin, line)) {
        row.clear();
        stringstream s(line);
        while(getline(s, word, ','))
            row.push_back(word);
        temp.id = stoi(row[0]);
        strcpy(temp.fn, row[1].c_str());
        strcpy(temp.ln, row[2].c_str());
        strcpy(temp.ssn, row[3].c_str());
        strcpy(temp.un, row[4].c_str());
        strcpy(temp.pw, row[5].c_str());
        insert(temp);
    }
}

int main() {
    char diskName[] = "disk";
    int choice;
    clock_t start;

    cout << "enter 1 to build new tree, 2 to load from disk, or 3 to quit: ";
    cin >> choice;
    if(choice == 1) {
        initSuper();
        make_disk(diskName);
        open_disk(diskName);
        cout << "reading records from " << DATA_NAME << "...\n";
        start = clock();
        readRecords();
        cout << "time took = " << float(clock() - start) / CLOCKS_PER_SEC << "s\n";
        cout << "depth = " << s.depth << endl << endl;
    } else if(choice == 2) {
        open_disk(diskName);
        readSuper();
    }
    else return 0;

    while(1) {
        cout << "enter 1 to search, 2 to insert, or 3 to quit: ";
        cin >> choice;
        if(choice == 1) {
            cout << "enter id: ";
            cin >> choice;
            start = clock();
            record r;
            bool found = search(choice, &r);
            cout << "time took = " << float(clock() - start) / CLOCKS_PER_SEC << "s\n";
            cout << "depth = " << s.depth << endl;
            if(found) {
                cout << "id = " << r.id << endl;
                cout << "first name = " << r.fn << endl;
                cout << "last name = " << r.ln << endl;
                cout << "ssn = " << r.ssn << endl;
                cout << "username = " << r.un << endl;
                cout << "password = " << r.pw << endl;
            }
        } else if(choice == 2) {
            record r;
            cout << "enter id: ";
            cin >> r.id;
            cout << "enter first name: ";
            cin >> r.fn;
            cout << "enter last name: ";
            cin >> r.ln;
            cout << "enter ssn: ";
            cin >> r.ssn;
            cout << "enter username: ";
            cin >> r.un;
            cout << "enter password: ";
            cin >> r.pw;
            start = clock();
            insert(r);
            cout << "time took = " << float(clock() - start) / CLOCKS_PER_SEC << "s\n";
            cout << "depth = " << s.depth << endl << endl;
        } else {
            writeSuper();
            close_disk();
            return 0;
        }
    }
}