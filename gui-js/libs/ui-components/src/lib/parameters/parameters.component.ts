import { Component } from '@angular/core';
import { ElectronService } from '@minsky/core';
import { ActivatedRoute } from '@angular/router';
import { switchMap } from 'rxjs/operators';
import { ScaleHandler } from '../scale-handler/scale-handler.class';
import { uniqBy, sortBy } from 'lodash-es';
import * as JSON5 from 'json5';

@Component({
  selector: 'minsky-parameters',
  templateUrl: './parameters.component.html',
  styleUrls: ['./parameters.component.scss'],
})
export class ParametersComponent {
  variables;

  tensorText = '<tensor>';

  type: string;
  types: string[];

  scale = new ScaleHandler();

  constructor(private electronService: ElectronService, route: ActivatedRoute) {
    route.params.pipe(switchMap(p => {
      this.type = p['tab'];
      if(this.type === 'parameters') {
        return electronService.minsky.parameterTab.getDisplayVariables();
      } else {
        return electronService.minsky.variableTab.getDisplayVariables();
      }
    })).subscribe((v: any) => this.prepareVariables(v));
  }

  prepareVariables(variables: any[]) {
    this.variables={};
    for (let v in variables) {
      let type=variables[v].type;
      if (this.variables[type])
        this.variables[type].push(variables[v]);
      else
        this.variables[type]=[variables[v]];
    }

    this.types=Object.keys(this.variables);
    this.types.sort();
    for (let type in this.variables)
      this.variables[type].sort((x,y)=>{return x.name<y.name;});
    // why tf is this uniqBy necessary?! sort duplicates rows..? js' own .sort function does the same thing
    //  this.variables[type] = uniqBy(sortBy(variables[type], ['name']), v => v.name);
  }

  toggleCaret(event) {
    event.target.classList.toggle("caret-down");
    event.target.querySelector(".nested").classList.toggle("active");
  }
  
  changeScale(e) {
    if(e.ctrlKey) {
      this.scale.changeScale(e.deltaY);
    }
  }

  truncateValue(value: string) {
    if(isNaN(+value)) return value;
    const stringDecimals = String(+value - Math.floor(+value));
    if(stringDecimals.length > 6) return (+value).toFixed(4);
    return String(value);
  }
  
}
