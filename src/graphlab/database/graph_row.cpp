#include <graphlab/database/graph_row.hpp>
#include <graphlab/database/graph_database.hpp>

namespace graphlab {

int graph_row::get_field_pos(const char* fieldname) {
  int fieldpos = -1;
  if (is_vertex()) {
    fieldpos = _database->find_vertex_field(fieldname);
  } else {
    fieldpos = _database->find_edge_field(fieldname);
  }
  return fieldpos;
} 

graph_value* graph_row::get_field(size_t fieldpos) {
  if (fieldpos < _data.size()) return _data[fieldpos];
  else return NULL; 
}

graph_value* graph_row::get_field(const char* fieldname) {
  int fieldpos = get_field_pos(fieldname);
  if (fieldpos < 0) {
    return NULL;
  } else {
    graph_value* ret = get_field(fieldpos);
    // this cannot possibly be NULL. This means that there is a disagreement
    // between what the database thinks are the fields, and what the row
    // thinks are the fields
    assert(ret != NULL);
    return ret;
  }
}

std::string graph_row::get_field_metadata(size_t fieldpos) {
  if (is_vertex()) {
    const std::vector<graph_field>& fields = _database->get_vertex_fields();
    if (fieldpos < fields.size()) {
      return fields[fieldpos].name;
    } else {
      return "";
    }
  } else { 
    const std::vector<graph_field>& fields = _database->get_edge_fields();
    if (fieldpos < fields.size()) {
      return fields[fieldpos].name;
    } else {
      return "";
    }
  }
}


} // namespace graphlab