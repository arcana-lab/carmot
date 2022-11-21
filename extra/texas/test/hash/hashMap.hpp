const int DEFAULT_TABLE_SIZE = 8192;


class LinkedHashEntry 
{
    private:
        int key;
        void* value;
        LinkedHashEntry *next;

    public:
        LinkedHashEntry(int key, void* value) {
            this->key   = key;
            this->value = value;
            this->next  = nullptr;
        }

        int getKey() {
            return key;
        }

        void* getValue() {
            return value;
        }

        void setValue(void* value) {
            this->value = value;
        }

        LinkedHashEntry *getNext() {
            return next;
        }

        void setNext(LinkedHashEntry *next) {
            this->next = next;
        }
};



class HashMap {

    private:

        float threshold;
        int maxSize;
        int tableSize;
        int size;
        LinkedHashEntry **table;



        void resize() {
            int oldTableSize = tableSize;
            tableSize *= 2;
            maxSize = (int) (tableSize * threshold);
            LinkedHashEntry **oldTable = table;
            table = new LinkedHashEntry*[tableSize];
            for (int i = 0; i < tableSize; i++)
                table[i] = nullptr;
            size = 0;
            for (int hash = 0; hash < oldTableSize; hash++)
                if (oldTable[hash] != nullptr) {
                    LinkedHashEntry *oldEntry;
                    LinkedHashEntry *entry = oldTable[hash];
                    while (entry != nullptr) {
                        put(entry->getKey(), entry->getValue());
                        oldEntry = entry;
                        entry = entry->getNext();
                        delete oldEntry;
                    }
                }
            delete[] oldTable;
        }


    public:
        HashMap() {
            threshold = 0.75f;
            maxSize = 4096;
            tableSize = DEFAULT_TABLE_SIZE;
            size = 0;
            table = new LinkedHashEntry*[tableSize];
            for (int i = 0; i < tableSize; i++)
                table[i] = nullptr;
        }

        void setThreshold(float threshold) {
            this->threshold = threshold;
            maxSize = (int) (tableSize * threshold);
        }

        void* get(int key) {
            int hash = (key % tableSize);
            if (table[hash] == nullptr)
                return nullptr;
            else {
                LinkedHashEntry *entry = table[hash];
                while (entry != nullptr && entry->getKey() != key)
                    entry = entry->getNext();
                if (entry == nullptr)
                    return nullptr;
                else
                    return entry->getValue();
            }
        }


        void put(int key, void* value) {
            int hash = (key % tableSize);
            if (table[hash] == nullptr) {
                table[hash] = new LinkedHashEntry(key, value);
                size++;
            } else {
                LinkedHashEntry *entry = table[hash];
                while (entry->getNext() != nullptr)
                    entry = entry->getNext();
                if (entry->getKey() == key)
                    entry->setValue(value);
                else {
                    entry->setNext(new LinkedHashEntry(key, value));
                    size++;
                }
            }
            if (size >= maxSize)
                resize();
        }

        void remove(int key) {
            int hash = (key % tableSize);
            if (table[hash] != nullptr) {
                LinkedHashEntry *prevEntry = nullptr;
                LinkedHashEntry *entry = table[hash];
                while (entry->getNext() != nullptr && entry->getKey() != key) {
                    prevEntry = entry;
                    entry = entry->getNext();
                }

                if (entry->getKey() == key) {
                    if (prevEntry == nullptr) {
                        LinkedHashEntry *nextEntry = entry->getNext();
                        delete entry;
                        table[hash] = nextEntry;
                    } else {
                        LinkedHashEntry *next = entry->getNext();
                        delete entry;
                        prevEntry->setNext(next);
                    }
                    size--;
                }
            }
        }

        ~HashMap() {
            for (int hash = 0; hash < tableSize; hash++)
                if (table[hash] != nullptr) {
                    LinkedHashEntry *prevEntry = nullptr;
                    LinkedHashEntry *entry = table[hash];
                    while (entry != nullptr) {
                        prevEntry = entry;
                        entry = entry->getNext();
                        delete prevEntry;
                    }
                }
            delete[] table;
        }
};


