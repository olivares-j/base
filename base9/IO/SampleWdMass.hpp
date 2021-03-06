#ifndef IO_SAMPLE_WD_MASS_HPP
#define IO_SAMPLE_WD_MASS_HPP

#include "BackingStore.hpp"
#include "Records.hpp"


class SampleWdMass_FileBackingStore : public FileBackingStore<std::vector<SampleWdMassRecord>, SampleWdMassRecord>
{
  public:
    SampleWdMass_FileBackingStore(std::string);

    ~SampleWdMass_FileBackingStore() override = default;

    void save(std::vector<SampleWdMassRecord>) override;

  protected:
    void header(SampleWdMassRecord) override;

  private:
    std::ofstream membership;
    std::ofstream precLogAge;
    std::ofstream coolingAge;
    std::ofstream logTeff;
    std::ofstream logG;
};


class SampleWdMass_SqlBackingStore : public SqlBackingStore<std::vector<SampleWdMassRecord>>
{
  public:
    SampleWdMass_SqlBackingStore(const RunData&);
    SampleWdMass_SqlBackingStore(const SampleWdMass_SqlBackingStore&) = delete;
    SampleWdMass_SqlBackingStore(std::string);

    ~SampleWdMass_SqlBackingStore() override;

    void save(std::vector<SampleWdMassRecord>) override;

  private:
    sqlite3_stmt *insert = nullptr;

    void buildInsertStatement() override;
};

#endif
