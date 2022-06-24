/**
 * @file   test_dynamic_dictionary.cpp
 * @brief  Test unit for DynamicDictionary class
 * @author Lyndon Hill and others
 */

#include <stdlib.h>

#include <iostream>

#include "bedic.h"

int main()
{
  std::string errorMessage;
  
  DynamicDictionary *dic = createDynamicDictionary( "test.edic", "Test dictionary", errorMessage );

  if( dic == NULL ) {
    std::cerr << "Failed with error: " << errorMessage << "\n";
    return EXIT_FAILURE;
  } else {
    std::cerr << "Created dynamic dictionary\n";
  }

  const int N = 10;
  std::cerr << "Inserting " << N << " entries\n";
  for( int i = 0; i < N; i++ ) {
    std::string keyword;

    int l = rand() % 10 + 5;
    for( int j = 0; j < l; j++ )
      keyword += 'a' + (rand()%25);

    DictionaryIteratorPtr item = dic->insertEntry( keyword.c_str() );
    if( !item.isValid() ) {
      std::cerr << "Inserting " << keyword << " has failed because " << dic->getErrorMessage() << "\n";
      continue;
    }

    std::string description;
    l = rand() % 20 + 10;
    for( int j = 0; j < l; j++ )
      description += 'a' + (rand()%25);

    if( !dic->updateEntry( item, description.c_str() ) ) {
      std::cerr << "Failed with error: " << dic->getErrorMessage() << "\n";
      return EXIT_FAILURE;
    }
  }

  std::cerr << "Listing all entries\n";
  DictionaryIteratorPtr it = dic->begin();
  if( !it.isValid() ) {
    std::cerr << "Failed with error: " << dic->getErrorMessage() << "\n";
    return EXIT_FAILURE;
  }

  while( !(it == dic->end()) ) {
    std::cerr << "# " << it->getKeyword() << " - " << it->getDescription() << "\n";
    if( !it->nextEntry() ) {
      std::cerr << "Failed with error: " << dic->getErrorMessage() << "\n";
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
