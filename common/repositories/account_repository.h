#ifndef EQEMU_ACCOUNT_REPOSITORY_H
#define EQEMU_ACCOUNT_REPOSITORY_H

#include "../database.h"
#include "../strings.h"
#include "base/base_account_repository.h"

class AccountRepository: public BaseAccountRepository {
public:

    /**
     * This file was auto generated and can be modified and extended upon
     *
     * Base repository methods are automatically
     * generated in the "base" version of this repository. The base repository
     * is immutable and to be left untouched, while methods in this class
     * are used as extension methods for more specific persistence-layer
     * accessors or mutators.
     *
     * Base Methods (Subject to be expanded upon in time)
     *
     * Note: Not all tables are designed appropriately to fit functionality with all base methods
     *
     * InsertOne
     * UpdateOne
     * DeleteOne
     * FindOne
     * GetWhere(std::string where_filter)
     * DeleteWhere(std::string where_filter)
     * InsertMany
     * All
     *
     * Example custom methods in a repository
     *
     * AccountRepository::GetByZoneAndVersion(int zone_id, int zone_version)
     * AccountRepository::GetWhereNeverExpires()
     * AccountRepository::GetWhereXAndY()
     * AccountRepository::DeleteWhereXAndY()
     *
     * Most of the above could be covered by base methods, but if you as a developer
     * find yourself re-using logic for other parts of the code, its best to just make a
     * method that can be re-used easily elsewhere especially if it can use a base repository
     * method and encapsulate filters there
     */

	// Custom extended repository methods here

	static int UpdateGodModeFlags(
		Database &db,
		int32_t id,
		int8_t flymode,
		uint8_t gmspeed,
		int8_t gminvul,
		int8_t hideme
	)
	{
		std::vector<std::string> v;

		auto columns = Columns();

		v.push_back("gmspeed = " + std::to_string(gmspeed));
		v.push_back("hideme = " + std::to_string(hideme));
		v.push_back("gminvul = " + std::to_string(gminvul));
		v.push_back("flymode = " + std::to_string(flymode));

		auto results = db.QueryDatabase(
			fmt::format(
				"UPDATE {} SET {} WHERE {} = {}",
				TableName(),
				Strings::Implode(", ", v),
				PrimaryKey(),
				id
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}

};

#endif //EQEMU_ACCOUNT_REPOSITORY_H
