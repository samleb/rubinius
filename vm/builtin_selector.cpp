#include "builtin.hpp"

namespace rubinius {
  void Selector::init(STATE) {
    state->globals.selectors = LookupTable::create(state);
    Class* cls = state->new_class("Selector", Selector::fields);
    cls->set_object_type(SelectorType);

    state->globals.selector = cls;

    cls->set_const(state, state->symbol("ALL"), state->globals.selectors);
  }

  Selector* Selector::create(STATE, OBJECT name) {
    Selector* sel = (Selector*)state->new_object(state->globals.selector);
    SET(sel, name, name);
    SET(sel, send_sites, Array::create(state, 1));

    return sel;
  }

  Selector* Selector::lookup(STATE, OBJECT name) {
    Selector* sel = (Selector*)state->globals.selectors->fetch(state, name);
    if(!sel->nil_p()) return sel;

    sel = Selector::create(state, name);
    state->globals.selectors->store(state, name, sel);

    return sel;
  }

  OBJECT Selector::associate(STATE, SendSite* ss) {
    send_sites->append(state, ss);
    return ss;
  }

  bool Selector::includes_p(STATE, SendSite* ss) {
    return send_sites->includes_p(state, ss);
  }

  void Selector::clear(STATE) {
    size_t sz = send_sites->size();
    SendSite* ss;

    for(size_t i = 0; i < sz; i++) {
      ss = (SendSite*)send_sites->get(state, i);
      ss->initialize(state);
    }
  }

  void Selector::clear_by_name(STATE, OBJECT name) {
    Selector* sel = (Selector*)state->globals.selectors->fetch(state, name);
    if(sel->nil_p()) return;

    sel->clear(state);
  }
};