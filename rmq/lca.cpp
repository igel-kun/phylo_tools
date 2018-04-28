

#include <assert.h>
#include <iostream>
#include <string>

#include "tree.hpp"
#include "lca.hpp"
#include "pm_rmq.hpp"

using std::string;

void LCA_TEST(const lca<tree<string> >& lca, const tree<string>& u, const tree<string>& v, const string& expect){
  string ret = lca.query(u, v)->get_data();                                       
  if (expect != ret) {                                                
    std::cout << "expected LCA(" << u.get_data() << ", " << v.get_data() << ") = " << expect << std::endl; 
    std::cout << "got " << ret << std::endl;                          
  }                                                                   
  assert(ret == expect);                                              
}

int main(int argc, const char *argv[]) {
  unsigned index = 0;
  std::vector<tree<string> > a_children;
  {
    std::vector<tree<string> > b_children;
    b_children.push_back(tree<string>(index++, "c"));
    b_children.push_back(tree<string>(index++, "d"));
    b_children.push_back(tree<string>(index++, "e"));
    a_children.push_back(tree<string>(index++, "b", std::move(b_children)));
  }
  {
    std::vector<tree<string> > f_children;
    {
      std::vector<tree<string> > g_children;
      g_children.push_back(tree<string>(index++, "h"));
      f_children.push_back(tree<string>(index++, "g", std::move(g_children)));
    }
    f_children.push_back(tree<string>(index++, "i"));
    a_children.push_back(tree<string>(index++, "f", std::move(f_children)));
  }
  tree<string> input(index++, "a", std::move(a_children));
  lca<tree<string> > lca(input);


  LCA_TEST(lca, input, input, "a");
  LCA_TEST(lca, *input.children()[0], *input.children()[1], "a");
  LCA_TEST(lca, *input.children()[0]->children()[0], *input.children()[0]->children()[2], "b");
  LCA_TEST(lca, *input.children()[1]->children()[0]->children()[0], *input.children()[1]->children()[1], "f");

  return 0;
}



