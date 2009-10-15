/* Generated by Together */

#include "OrderedTask.h"
#include "TaskPoints/FAISectorStartPoint.hpp"
#include "TaskPoints/FAISectorFinishPoint.hpp"
#include "TaskPoints/FAISectorASTPoint.hpp"
#include "TaskPoints/FAICylinderASTPoint.hpp"
#include "TaskPoints/CylinderAATPoint.hpp"
#include "PathSolvers/TaskDijkstra.hpp"
#include "TaskSolvers/TaskMacCreadyTravelled.hpp"
#include "TaskSolvers/TaskMacCreadyRemaining.hpp"
#include "TaskSolvers/TaskMacCreadyTotal.hpp"
#include "TaskSolvers/TaskCruiseEfficiency.hpp"
#include "TaskSolvers/TaskBestMc.hpp"
#include "TaskSolvers/TaskMinTarget.hpp"
#include "TaskSolvers/TaskGlideRequired.hpp"
#include <assert.h>
#include <fstream>

void
OrderedTask::update_geometry() {

  task_projection.reset(tps[0]->getLocation());
  for (unsigned i=0; i<tps.size(); i++) {
    task_projection.scan_location(tps[i]->getLocation());
  }
//  task_projection.report();
  task_projection.update_fast();

  for (unsigned i=0; i<tps.size(); i++) {
    tps[i]->update_geometry();
    tps[i]->clear_boundary_points();
    tps[i]->default_boundary_points();
    tps[i]->prune_boundary_points();
    tps[i]->update_projection();
  }
}

////////// TIMES

double 
OrderedTask::scan_total_start_time(const AIRCRAFT_STATE &)
{
  return ts->get_state_entered().Time;
}

double 
OrderedTask::scan_leg_start_time(const AIRCRAFT_STATE &)
{
  if (activeTaskPoint>0) {
    return tps[activeTaskPoint-1]->get_state_entered().Time;
  } else {
    return -1;
  }
}

////////// DISTANCES

void
OrderedTask::scan_distance_minmax(const GEOPOINT &location, bool full,
                                  double *dmin, double *dmax)
{
  TaskDijkstra dijkstra(this, tps.size());
  ScanTaskPoint start(0,0);
  SearchPoint ac(location, task_projection);
  if (full) {
    // for max calculations, since one can still travel further in the sector,
    // we pretend we are on the previous turnpoint so the search samples will
    // contain the full boundary
    if (activeTaskPoint>0) {
      ts->scan_active(tps[activeTaskPoint-1]);
    }
    *dmax = dijkstra.distance_opt_achieved(ac, false);
    ts->scan_active(tps[activeTaskPoint]);
  }
  *dmin = dijkstra.distance_opt_achieved(ac, true);
}


double
OrderedTask::scan_distance_nominal()
{
  return ts->scan_distance_nominal();
}

double
OrderedTask::scan_distance_scored(const GEOPOINT &location)
{
  return ts->scan_distance_scored(location);
}

double
OrderedTask::scan_distance_remaining(const GEOPOINT &location)
{
  return ts->scan_distance_remaining(location);
}

double
OrderedTask::scan_distance_travelled(const GEOPOINT &location)
{
  return ts->scan_distance_travelled(location);
}

double
OrderedTask::scan_distance_planned()
{
  return ts->scan_distance_planned();
}

////////// TRANSITIONS

bool 
OrderedTask::check_transitions(const AIRCRAFT_STATE &state, 
                               const AIRCRAFT_STATE &state_last)
{
  ts->scan_active(tps[activeTaskPoint]);

  const int n_task = tps.size();

  if (!n_task) {
    return false;
  }

  const int t_min = std::max(0,(int)activeTaskPoint-1);
  const int t_max = std::min(n_task-1, (int)activeTaskPoint+1);
  bool full_update = false;
  
  for (int i=t_min; i<=t_max; i++) {
    if (tps[i]->transition_enter(state, state_last)) {
      task_events.transition_enter(*tps[i]);
    }
    if (tps[i]->transition_exit(state, state_last)) {
      task_events.transition_exit(*tps[i]);
      if (i+1<n_task) {
        setActiveTaskPoint(i+1);
        ts->scan_active(tps[activeTaskPoint]);

        task_events.active_advanced(*tps[activeTaskPoint],i+1);

        // on sector exit, must update samples since start sector
        // exit transition clears samples
        full_update = true;
      }
    }
    if (tps[i]->update_sample(state)) {
      full_update = true;
    }
  }

  ts->scan_active(tps[activeTaskPoint]);

  return full_update;
}

////////// ADDITIONAL FUNCTIONS

bool 
OrderedTask::update_idle(const AIRCRAFT_STATE& state)
{
  double mc=2.0;
  // TODO get from above
  double p = calc_min_target(state, mc, 3600*5.0);

  TaskGlideRequired bgr(tps, activeTaskPoint, state);
  double S = bgr.search(mc);

  (void)p;
  (void)S;
  
  return true;
}


bool 
OrderedTask::update_sample(const AIRCRAFT_STATE &state, 
                           const bool full_update)
{
  return true;
}

////////// TASK 

void
OrderedTask::remove(unsigned position)
{
  assert(position>0);
  assert(position<tps.size()-1);

  if (activeTaskPoint>position) {
    activeTaskPoint--;
  }

  delete legs[position-1]; 
  delete legs[position];   // 01,12,23,34 -> 01,34 
  legs.erase(legs.begin()+position-1); // 01,12,23,34 -> 01,23,34 
  legs.erase(legs.begin()+position-1); // 01,12,23,34 -> 01,34 

  delete tps[position];
  tps.erase(tps.begin()+position); // 0,1,2,3 -> 0,1,3

  legs.insert(legs.begin()+position-1,
              new TaskLeg(*tps[position-1],*tps[position])); // 01,13,34

  update_geometry();
}

void 
OrderedTask::insert(OrderedTaskPoint* new_tp, unsigned position)
{
  // remove legs first
  assert(position>0);

  if (activeTaskPoint>=position) {
    activeTaskPoint++;
  }

  delete legs[position-1];
  legs.erase(legs.begin()+position-1); // 0,1,2 -> 0,2

  tps.insert(tps.begin()+position, new_tp); // 0,1,2 -> 0,1,N,2

  legs.insert(legs.begin()+position-1,
              new TaskLeg(*tps[position-1],*tps[position])); // 0,1N,2

  legs.insert(legs.begin()+position,
              new TaskLeg(*tps[position],*tps[position+1])); // 0,1N,N2,2
  
  update_geometry();
}

//////////  

void OrderedTask::setActiveTaskPoint(unsigned index)
{
  if (index<tps.size()) {
    activeTaskPoint = index;
  }
}

TaskPoint* OrderedTask::getActiveTaskPoint()
{
  if (activeTaskPoint<tps.size()) {
    return tps[activeTaskPoint];
  } else {
    return NULL;
  }
}

////////// Constructors/destructors

OrderedTask::~OrderedTask()
{
// TODO: delete legs and turnpoints
}

OrderedTask::OrderedTask(const TaskEvents &te):
  AbstractTask(te)
{
  // TODO: default values in constructor

  WAYPOINT wp[6];
  wp[0].Location.Longitude=0;
  wp[0].Location.Latitude=0;
  wp[0].Altitude=0.25;
  wp[1].Location.Longitude=0;
  wp[1].Location.Latitude=1.0;
  wp[1].Altitude=0.25;
  wp[2].Location.Longitude=1.0;
  wp[2].Location.Latitude=1.0;
  wp[2].Altitude=0.5;
  wp[3].Location.Longitude=0.8;
  wp[3].Location.Latitude=0.5;
  wp[3].Altitude=0.25;
  wp[4].Location.Longitude=1.0;
  wp[4].Location.Latitude=0;
  wp[4].Altitude=0.25;

  ts = new FAISectorStartPoint(task_projection,wp[0]);
  tps.push_back(ts);
  tps.push_back(new FAISectorASTPoint(task_projection,wp[1]));
  tps.push_back(new CylinderAATPoint(task_projection,wp[2]));
  tps.push_back(new CylinderAATPoint(task_projection,wp[3]));
//  tps.push_back(new FAISectorASTPoint(task_projection,wp[4]));
//  tps.push_back(new FAISectorFinishPoint(task_projection,wp[0]));
  tps.push_back(new FAISectorFinishPoint(task_projection,wp[4]));

  for (unsigned i=0; i+1<tps.size(); i++) {
    legs.push_back(new TaskLeg(*tps[i],*tps[i+1]));
  }

  update_geometry();
}

////////// Glide functions

void
OrderedTask::glide_solution_remaining(const AIRCRAFT_STATE &aircraft, 
                                      const double mc,
                                      GLIDE_RESULT &total,
                                      GLIDE_RESULT &leg)
{
  TaskMacCreadyRemaining tm(tps,activeTaskPoint, mc);
  total = tm.glide_solution(aircraft);
  leg = tm.get_active_solution();

  std::ofstream fr("res-sol-remaining.txt");
  tm.print(fr, aircraft);
}

void
OrderedTask::glide_solution_travelled(const AIRCRAFT_STATE &aircraft, 
                                      const double mc,
                                      GLIDE_RESULT &total,
                                      GLIDE_RESULT &leg)
{
  TaskMacCreadyTravelled tm(tps,activeTaskPoint, mc);
  total = tm.glide_solution(aircraft);
  leg = tm.get_active_solution();

  std::ofstream fr("res-sol-travelled.txt");
  tm.print(fr, aircraft);
}

void
OrderedTask::glide_solution_planned(const AIRCRAFT_STATE &aircraft, 
                                    const double mc,
                                    GLIDE_RESULT &total,
                                    GLIDE_RESULT &leg,
                                    DistanceRemainingStat &total_remaining_effective,
                                    DistanceRemainingStat &leg_remaining_effective,
                                    const double total_t_elapsed,
                                    const double leg_t_elapsed)
{
  TaskMacCreadyTotal tm(tps,activeTaskPoint, mc);
  total = tm.glide_solution(aircraft);
  leg = tm.get_active_solution();

  std::ofstream fr("res-sol-planned.txt");
  tm.print(fr, aircraft);

  total_remaining_effective.
    set_distance(tm.effective_distance(total_t_elapsed));

  leg_remaining_effective.
    set_distance(tm.effective_leg_distance(leg_t_elapsed));

}

////////// Auxiliary glide functions

double
OrderedTask::calc_mc_best(const AIRCRAFT_STATE &aircraft, 
                          const double mc)
{
  TaskBestMc bmc(tps,activeTaskPoint, aircraft);
  return bmc.search(mc);
}


double
OrderedTask::calc_cruise_efficiency(const AIRCRAFT_STATE &aircraft, 
                                    const double mc)
{
  TaskCruiseEfficiency bce(tps,activeTaskPoint, aircraft, mc);
  return bce.search(mc);
}

double
OrderedTask::calc_min_target(const AIRCRAFT_STATE &aircraft, 
                             const double mc,
                             const double t_target)
{
  // TODO: look at max/min dist and only perform this scan if
  // change is possible
  const double t_rem = std::max(0.0, t_target-stats.total.TimeElapsed);

  TaskMinTarget bmt(tps, activeTaskPoint, aircraft, t_rem, ts);
  double p= bmt.search(mc);
//  printf("target opt %g\n",p);
  return p;
}


////////// Reporting/printing for debugging


void OrderedTask::report(const AIRCRAFT_STATE &state) 
{
  AbstractTask::report(state);

  std::ofstream f1("res-task.txt");

  f1 << "#### Task points\n";
  for (unsigned i=0; i<tps.size(); i++) {
    f1 << "## point " << i << "\n";
    tps[i]->print(f1);
  }

  std::ofstream f5("res-ssample.txt");
  f5 << "#### Task sampled points\n";
  for (unsigned i=0; i<tps.size(); i++) {
    f5 << "## point " << i << "\n";
    tps[i]->print_samples(f5);
  }

  std::ofstream f2("res-max.txt");
  f2 << "#### Max task\n";
  for (unsigned i=0; i<tps.size(); i++) {
    OrderedTaskPoint *tp = tps[i];
    f2 <<  tp->getMaxLocation().Longitude << " " 
       <<  tp->getMaxLocation().Latitude << "\n";
  }

  std::ofstream f3("res-min.txt");
  f3 << "#### Min task\n";
  for (unsigned i=0; i<tps.size(); i++) {
    OrderedTaskPoint *tp = tps[i];
    f3 <<  tp->getMinLocation().Longitude << " " 
       <<  tp->getMinLocation().Latitude << "\n";
  }
}

